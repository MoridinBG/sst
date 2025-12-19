#include "lsm6dso.h"
#include "../fw/hardware_config.h"
#include "hardware/gpio.h"
#include "pico/error.h"
#include "pico/time.h"
#include <stdio.h>

#ifdef IMU_SPI
#include "hardware/spi.h"
#else
#include "hardware/i2c.h"
#endif

#define WHO_AM_I 0x0F
#define CTRL1_XL 0x10
#define CTRL2_G  0x11
#define CTRL3_C  0x12
#define CTRL4_C  0x13
#define OUTX_L_G 0x22

#ifdef IMU_SPI

#define WRITE_MASK 0x7F
#define READ_MASK  0x80

static void cs_select(void) { gpio_put(IMU_PIN_CS, 0); }

static void cs_deselect(void) { gpio_put(IMU_PIN_CS, 1); }

static void write_register(uint8_t reg, uint8_t data) {
    uint8_t buf[2];
    buf[0] = reg & WRITE_MASK;
    buf[1] = data;
    cs_select();
    spi_write_blocking(IMU_SPI_INST, buf, 2);
    cs_deselect();
}

static uint8_t read_register(uint8_t reg) {
    uint8_t buf[1];
    uint8_t reg_addr = reg | READ_MASK;
    cs_select();
    spi_write_blocking(IMU_SPI_INST, &reg_addr, 1);
    spi_read_blocking(IMU_SPI_INST, 0, buf, 1);
    cs_deselect();
    return buf[0];
}

static void read_registers(uint8_t reg, uint8_t *buf, size_t len) {
    uint8_t reg_addr = reg | READ_MASK;
    cs_select();
    spi_write_blocking(IMU_SPI_INST, &reg_addr, 1);
    spi_read_blocking(IMU_SPI_INST, 0, buf, len);
    cs_deselect();
}

int lsm6dso_init(void) {
    // Initialize SPI bus
    spi_init(IMU_SPI_INST, 1000000);
    spi_set_format(IMU_SPI_INST, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_set_function(IMU_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(IMU_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(IMU_PIN_MISO, GPIO_FUNC_SPI);

    // Initialize CS pin
    gpio_init(IMU_PIN_CS);
    gpio_set_dir(IMU_PIN_CS, GPIO_OUT);
    gpio_put(IMU_PIN_CS, 1);

    // Wait for sensor to boot
    sleep_ms(10);

    // Software reset
    printf("[LSM6DSO] Resetting...\n");
    write_register(CTRL3_C, 0x01);
    sleep_ms(10);

    // Check ID with retries
    uint8_t id = 0;
    for (int i = 0; i < 5; i++) {
        id = read_register(WHO_AM_I);
        printf("[LSM6DSO] WHO_AM_I: 0x%02x (expected 0x6C)\n", id);
        if (id == 0x6C)
            break;
        sleep_ms(10);
    }

    if (id != 0x6C) {
        return PICO_ERROR_NOT_FOUND;
    }

    // Disable I2C interface (SPI mode only)
    write_register(CTRL4_C, 0x04);

    // Control setup (BDU=1, IF_INC=1)
    write_register(CTRL3_C, 0x44);

    // Accel setup (ODR=1.66kHz, FS=8g)
    write_register(CTRL1_XL, 0x8C);

    // Gyro setup (ODR=1.66kHz, FS=1000dps)
    write_register(CTRL2_G, 0x88);

    return PICO_OK;
}

#else // IMU_I2C

static void write_register(uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    i2c_write_blocking(IMU_I2C_INST, IMU_ADDRESS, buf, 2, false);
}

static uint8_t read_register(uint8_t reg) {
    uint8_t buf[1];
    i2c_write_blocking(IMU_I2C_INST, IMU_ADDRESS, &reg, 1, true);
    i2c_read_blocking(IMU_I2C_INST, IMU_ADDRESS, buf, 1, false);
    return buf[0];
}

static void read_registers(uint8_t reg, uint8_t *buf, size_t len) {
    i2c_write_blocking(IMU_I2C_INST, IMU_ADDRESS, &reg, 1, true);
    i2c_read_blocking(IMU_I2C_INST, IMU_ADDRESS, buf, len, false);
}

int lsm6dso_init(void) {
    // Initialize I2C bus at 1MHz (fast mode plus)
    i2c_init(IMU_I2C_INST, 1000000);
    gpio_set_function(IMU_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(IMU_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(IMU_PIN_SDA);
    gpio_pull_up(IMU_PIN_SCL);

    // Wait for sensor to boot
    sleep_ms(10);

    // Software reset
    printf("[LSM6DSO] Resetting...\n");
    write_register(CTRL3_C, 0x01);
    sleep_ms(10);

    // Check ID with retries
    uint8_t id = 0;
    for (int i = 0; i < 5; i++) {
        id = read_register(WHO_AM_I);
        printf("[LSM6DSO] WHO_AM_I: 0x%02x (expected 0x6C)\n", id);
        if (id == 0x6C)
            break;
        sleep_ms(10);
    }

    if (id != 0x6C) {
        return PICO_ERROR_NOT_FOUND;
    }

    // Control setup (BDU=1, IF_INC=1)
    write_register(CTRL3_C, 0x44);

    // Accel setup (ODR=1.66kHz, FS=8g)
    write_register(CTRL1_XL, 0x8C);

    // Gyro setup (ODR=1.66kHz, FS=1000dps)
    write_register(CTRL2_G, 0x88);

    return PICO_OK;
}

#endif // IMU_SPI

void lsm6dso_read(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz) {
    uint8_t buffer[12];
    read_registers(OUTX_L_G, buffer, 12);

    *gx = (int16_t)(buffer[1] << 8 | buffer[0]);
    *gy = (int16_t)(buffer[3] << 8 | buffer[2]);
    *gz = (int16_t)(buffer[5] << 8 | buffer[4]);
    *ax = (int16_t)(buffer[7] << 8 | buffer[6]);
    *ay = (int16_t)(buffer[9] << 8 | buffer[8]);
    *az = (int16_t)(buffer[11] << 8 | buffer[10]);
}
