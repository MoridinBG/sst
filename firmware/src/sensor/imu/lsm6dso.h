#ifndef LSM6DSO_H
#define LSM6DSO_H

#include <stdbool.h>
#include <stdint.h>

struct imu_sensor;

// Initialize hardware (SPI or I2C based on imu->protocol)
// Sets up GPIOs, resets sensor, checks WHO_AM_I, configures ODR/FS
// Sets imu->available = true on success, false on failure
void lsm6dso_init(struct imu_sensor *imu);

// Check if sensor responds (WHO_AM_I check)
bool lsm6dso_check_availability(struct imu_sensor *imu);

// Read raw 6-axis data: ax,ay,az,gx,gy,gz
void lsm6dso_read_raw(struct imu_sensor *imu, int16_t raw[6]);

// Read temperature (raw 16-bit value, 256 LSB/degC, zero at 25degC)
int16_t lsm6dso_read_temperature(struct imu_sensor *imu);

// Convert the raw value to degrees celsius
float lsm6dso_temperature_celsius(struct imu_sensor *imu);

#endif // LSM6DSO_H