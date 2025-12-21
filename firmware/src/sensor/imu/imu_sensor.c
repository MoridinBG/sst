#include "imu_sensor.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <pico/stdlib.h>

// Static buffer for forward motion calibration - only used during calibration
// 300 samples at 100Hz = 3 seconds of recording
#define IMU_CAL_BUFFER_SIZE 300
static int16_t (*cal_buffer)[6] = NULL;
static uint16_t cal_buffer_count = 0;
static uint16_t cal_buffer_head = 0;
static float g_sensor[3]; // Gravity vector from stationary calibration

bool imu_sensor_init(struct imu_sensor *imu) {
    if (imu->init) {
        imu->init(imu);
    }
    return imu->available;
}

bool imu_sensor_available(struct imu_sensor *imu) {
    if (imu->check_availability) {
        return imu->check_availability(imu);
    }
    return false;
}

void imu_sensor_read_raw(struct imu_sensor *imu, int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy,
                         int16_t *gz) {
    int16_t raw[6];
    if (imu->read_raw) {
        imu->read_raw(imu, raw);
        *ax = raw[0];
        *ay = raw[1];
        *az = raw[2];
        *gx = raw[3];
        *gy = raw[4];
        *gz = raw[5];
    } else {
        *ax = *ay = *az = *gx = *gy = *gz = 0;
    }
}

void imu_sensor_read(struct imu_sensor *imu, int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy,
                     int16_t *gz) {
    // Read raw values
    int16_t raw[6];
    if (imu->read_raw) {
        imu->read_raw(imu, raw);
    } else {
        memset(raw, 0, sizeof(raw));
    }

    // Calculate temperature difference from calibration point if supported
    float temp_diff = 0.0f;
    if (imu->read_temperature && imu->temp_scale > 0.0f) {
        int16_t current_temp = imu->read_temperature(imu);
        temp_diff = (float)(current_temp - imu->calibration.cal_temperature) / imu->temp_scale;
    }

    // Apply temperature-compensated bias
    float a[3], g[3];
    for (int i = 0; i < 3; i++) {
        a[i] = raw[i]   - imu->calibration.accel_bias[i] - imu->accel_temp_coeff * temp_diff;
        g[i] = raw[3+i] - imu->calibration.gyro_bias[i]  - imu->gyro_temp_coeff * temp_diff;
    }

    // Apply rotation matrix: bike = R x sensor
    float *R = &imu->calibration.rotation.matrix[0][0];

    *ax = (int16_t)(R[0] * a[0] + R[1] * a[1] + R[2] * a[2]);
    *ay = (int16_t)(R[3] * a[0] + R[4] * a[1] + R[5] * a[2]);
    *az = (int16_t)(R[6] * a[0] + R[7] * a[1] + R[8] * a[2]);

    *gx = (int16_t)(R[0] * g[0] + R[1] * g[1] + R[2] * g[2]);
    *gy = (int16_t)(R[3] * g[0] + R[4] * g[1] + R[5] * g[2]);
    *gz = (int16_t)(R[6] * g[0] + R[7] * g[1] + R[8] * g[2]);
}

float imu_sensor_get_temperature_celsius(struct imu_sensor *imu) {
    if (imu->temperature_celsius) {
        return imu->temperature_celsius(imu);
    }
    if (imu->read_temperature && imu->temp_scale > 0.0f) {
        int16_t raw = imu->read_temperature(imu);
        return imu->temp_offset + (float)raw / imu->temp_scale;
    }
    return 0.0f;
}

void imu_sensor_calibrate_stationary(struct imu_sensor *imu) {
    int32_t gyro_sum[3] = {0};
    int32_t accel_sum[3] = {0};

    // Record temperature at start of calibration
    if (imu->read_temperature) {
        imu->calibration.cal_temperature = imu->read_temperature(imu);
    }

    for (int i = 0; i < 100; i++) {
        int16_t raw[6];
        if (imu->read_raw) {
            imu->read_raw(imu, raw);
        } else {
            memset(raw, 0, sizeof(raw));
        }
        accel_sum[0] += raw[0]; // ax
        accel_sum[1] += raw[1]; // ay
        accel_sum[2] += raw[2]; // az
        gyro_sum[0] += raw[3];  // gx
        gyro_sum[1] += raw[4];  // gy
        gyro_sum[2] += raw[5];  // gz
        sleep_ms(10);
    }

    imu->calibration.gyro_bias[0] = gyro_sum[0] / 100;
    imu->calibration.gyro_bias[1] = gyro_sum[1] / 100;
    imu->calibration.gyro_bias[2] = gyro_sum[2] / 100;

    // Store gravity vector (will be normalized later)
    g_sensor[0] = accel_sum[0] / 100.0f;
    g_sensor[1] = accel_sum[1] / 100.0f;
    g_sensor[2] = accel_sum[2] / 100.0f;
}

bool imu_sensor_calibrate_forward_start(struct imu_sensor *imu) {
    if (cal_buffer) {
        free(cal_buffer);
    }
    cal_buffer = malloc(IMU_CAL_BUFFER_SIZE * sizeof(*cal_buffer));
    cal_buffer_count = 0;
    cal_buffer_head = 0;
    return cal_buffer != NULL;
}

bool imu_sensor_calibrate_forward_sample(struct imu_sensor *imu) {
    if (!cal_buffer) {
        return false;
    }

    // Write to current head position
    if (imu->read_raw) {
        imu->read_raw(imu, cal_buffer[cal_buffer_head]);
    } else {
        memset(cal_buffer[cal_buffer_head], 0, sizeof(cal_buffer[0]));
    }
    
    // Advance head (circular buffer)
    cal_buffer_head = (cal_buffer_head + 1) % IMU_CAL_BUFFER_SIZE;
    
    // Track count up to full size
    if (cal_buffer_count < IMU_CAL_BUFFER_SIZE) {
        cal_buffer_count++;
    }
    
    return true;
}

static void normalize(float *v, float *out) {
    float mag = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (mag > 0.0001f) {
        out[0] = v[0] / mag;
        out[1] = v[1] / mag;
        out[2] = v[2] / mag;
    } else {
        out[0] = 0;
        out[1] = 0;
        out[2] = 0;
    }
}

static float dot_product(float *a, float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

static void cross_product(float *a, float *b, float *out) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static void find_forward_direction(int16_t (*buffer)[6], uint16_t count, float g_sensor[3], float f_sensor[3]) {
    // Accumulate all acceleration vectors (gravity-subtracted)
    float accel_sum[3] = {0};

    for (uint16_t s = 0; s < count; s++) {
        // Get acceleration, subtract gravity
        accel_sum[0] += buffer[s][0] - g_sensor[0];
        accel_sum[1] += buffer[s][1] - g_sensor[1];
        accel_sum[2] += buffer[s][2] - g_sensor[2];
    }

    // Calculate magnitude of average acceleration
    float mag = sqrtf(accel_sum[0] * accel_sum[0] + accel_sum[1] * accel_sum[1] + accel_sum[2] * accel_sum[2]);

    // Output forward direction (normalized)
    if (mag > 0.0001f) {
        f_sensor[0] = accel_sum[0] / mag;
        f_sensor[1] = accel_sum[1] / mag;
        f_sensor[2] = accel_sum[2] / mag;
    } else {
        f_sensor[0] = 0;
        f_sensor[1] = 0;
        f_sensor[2] = 0;
    }
}

static void build_rotation_matrix(float g_sensor[3], float f_sensor[3], struct imu_rotation *rot) {
    // g_sensor = gravity vector in sensor frame (points UP in bike frame)
    // f_sensor = forward acceleration in sensor frame (points FORWARD in bike frame)

    // Normalize gravity -> this is bike's Z axis in sensor coords
    float z[3];
    normalize(g_sensor, z);

    // Remove Z component from forward vector, normalize -> bike's X axis
    float x[3];
    float dot_fz = dot_product(f_sensor, z);
    for (int i = 0; i < 3; i++) { x[i] = f_sensor[i] - dot_fz * z[i]; }
    normalize(x, x);

    // Y = Z x X (cross product) -> bike's Y axis (left)
    float y[3];
    cross_product(z, x, y);
    // Already normalized since Z and X are orthonormal

    // Build rotation matrix (rows are basis vectors)
    // Row 0 = X (forward), Row 1 = Y (left), Row 2 = Z (up)
    for (int i = 0; i < 3; i++) {
        rot->matrix[0][i] = x[i];
        rot->matrix[1][i] = y[i];
        rot->matrix[2][i] = z[i];
    }
}

void imu_sensor_calibrate_forward_finish(struct imu_sensor *imu) {
    if (!cal_buffer) {
        return;
    }

    float f_sensor[3];
    find_forward_direction(cal_buffer, cal_buffer_count, g_sensor, f_sensor);
    build_rotation_matrix(g_sensor, f_sensor, &imu->calibration.rotation);

    free(cal_buffer);
    cal_buffer = NULL;
}
