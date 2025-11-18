#ifndef MPU6886_H
#define MPU6886_H

#include "esp_err.h"
#include <stdbool.h>

// M5StickC Plus2 I2C pins for IMU
#define IMU_I2C_SDA     21
#define IMU_I2C_SCL     22
#define MPU6886_ADDR    0x68

// Accelerometer data structure
typedef struct {
    float x;  // g-force
    float y;
    float z;
} accel_data_t;

// Gyroscope data structure
typedef struct {
    float x;  // degrees/second
    float y;
    float z;
} gyro_data_t;

// Initialize MPU6886
esp_err_t mpu6886_init(void);

// Read accelerometer data (in g-force)
esp_err_t mpu6886_get_accel(accel_data_t *data);

// Read gyroscope data (in degrees/second)
esp_err_t mpu6886_get_gyro(gyro_data_t *data);

// Check if device is shaking (magnitude above threshold)
bool mpu6886_is_shaking(float threshold);

// Get total acceleration magnitude
float mpu6886_get_magnitude(void);

#endif // MPU6886_H
