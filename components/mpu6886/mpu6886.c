#include "mpu6886.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "MPU6886";

#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  400000

// MPU6886 Registers
#define MPU6886_PWR_MGMT_1      0x6B
#define MPU6886_PWR_MGMT_2      0x6C
#define MPU6886_SMPLRT_DIV      0x19
#define MPU6886_CONFIG          0x1A
#define MPU6886_GYRO_CONFIG     0x1B
#define MPU6886_ACCEL_CONFIG    0x1C
#define MPU6886_ACCEL_CONFIG2   0x1D
#define MPU6886_ACCEL_XOUT_H    0x3B
#define MPU6886_GYRO_XOUT_H     0x43
#define MPU6886_WHO_AM_I        0x75

// Scaling factors
#define ACCEL_SCALE     (1.0f / 8192.0f)  // ±4g range
#define GYRO_SCALE      (1.0f / 131.0f)   // ±250 dps range

static bool initialized = false;
static accel_data_t last_accel = {0};

// Write a byte to register
static esp_err_t mpu6886_write_byte(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, MPU6886_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

// Read bytes from register
static esp_err_t mpu6886_read_bytes(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, MPU6886_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

esp_err_t mpu6886_init(void)
{
    ESP_LOGI(TAG, "Initializing MPU6886...");

    // Configure I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = IMU_I2C_SDA,
        .scl_io_num = IMU_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Check WHO_AM_I register
    uint8_t who_am_i = 0;
    ret = mpu6886_read_bytes(MPU6886_WHO_AM_I, &who_am_i, 1);
    if (ret != ESP_OK || who_am_i != 0x19) {
        ESP_LOGE(TAG, "WHO_AM_I check failed: got 0x%02X, expected 0x19", who_am_i);
        return ESP_ERR_NOT_FOUND;
    }

    // Reset device
    mpu6886_write_byte(MPU6886_PWR_MGMT_1, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Wake up (disable sleep)
    mpu6886_write_byte(MPU6886_PWR_MGMT_1, 0x01);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Configure accelerometer (±4g)
    mpu6886_write_byte(MPU6886_ACCEL_CONFIG, 0x08);

    // Configure gyroscope (±250 dps)
    mpu6886_write_byte(MPU6886_GYRO_CONFIG, 0x00);

    // Set sample rate divider
    mpu6886_write_byte(MPU6886_SMPLRT_DIV, 0x00);

    // Configure DLPF
    mpu6886_write_byte(MPU6886_CONFIG, 0x01);
    mpu6886_write_byte(MPU6886_ACCEL_CONFIG2, 0x01);

    initialized = true;
    ESP_LOGI(TAG, "MPU6886 initialized successfully");
    return ESP_OK;
}

esp_err_t mpu6886_get_accel(accel_data_t *data)
{
    if (!initialized || !data) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[6];
    esp_err_t ret = mpu6886_read_bytes(MPU6886_ACCEL_XOUT_H, buf, 6);
    if (ret != ESP_OK) {
        return ret;
    }

    int16_t raw_x = (buf[0] << 8) | buf[1];
    int16_t raw_y = (buf[2] << 8) | buf[3];
    int16_t raw_z = (buf[4] << 8) | buf[5];

    data->x = raw_x * ACCEL_SCALE;
    data->y = raw_y * ACCEL_SCALE;
    data->z = raw_z * ACCEL_SCALE;

    last_accel = *data;
    return ESP_OK;
}

esp_err_t mpu6886_get_gyro(gyro_data_t *data)
{
    if (!initialized || !data) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[6];
    esp_err_t ret = mpu6886_read_bytes(MPU6886_GYRO_XOUT_H, buf, 6);
    if (ret != ESP_OK) {
        return ret;
    }

    int16_t raw_x = (buf[0] << 8) | buf[1];
    int16_t raw_y = (buf[2] << 8) | buf[3];
    int16_t raw_z = (buf[4] << 8) | buf[5];

    data->x = raw_x * GYRO_SCALE;
    data->y = raw_y * GYRO_SCALE;
    data->z = raw_z * GYRO_SCALE;

    return ESP_OK;
}

float mpu6886_get_magnitude(void)
{
    accel_data_t data;
    if (mpu6886_get_accel(&data) != ESP_OK) {
        return 0.0f;
    }
    return sqrtf(data.x * data.x + data.y * data.y + data.z * data.z);
}

bool mpu6886_is_shaking(float threshold)
{
    float mag = mpu6886_get_magnitude();
    // At rest, magnitude is ~1.0g. Shaking adds to this.
    return fabsf(mag - 1.0f) > threshold;
}
