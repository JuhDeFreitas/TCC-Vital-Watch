#include "mpu6050/mpu6050.h"
#include "mpu6050/mpu6050_config.h"
#include "i2c/i2c.h"
#include "i2c/i2c_config.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

static const char *TAG = "MPU6050";

esp_err_t mpu6050_write_byte(uint8_t reg, uint8_t data)
{
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_write_to_device(
        I2C_MASTER_NUM,
        MPU6050_ADDR,
        write_buf,
        sizeof(write_buf),
        pdMS_TO_TICKS(1000)
    );
}

esp_err_t mpu6050_read_bytes(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(
        I2C_MASTER_NUM,
        MPU6050_ADDR,
        &reg,
        1,
        data,
        len,
        pdMS_TO_TICKS(1000)
    );
}

void start_mpu6050(void *pvParameters)
{
    //ESP_ERROR_CHECK(i2c_master_init());
    //ESP_LOGI(TAG, "I2C inicializado");

    // Tira o MPU6050 do sleep
    ESP_ERROR_CHECK(mpu6050_write_byte(MPU6050_REG_PWR_MGMT_1, 0x00));
    ESP_LOGI(TAG, "MPU6050 acordado");

    uint8_t who_am_i = 0;
    ESP_ERROR_CHECK(mpu6050_read_bytes(MPU6050_REG_WHO_AM_I, &who_am_i, 1));
    ESP_LOGI(TAG, "WHO_AM_I = 0x%02X", who_am_i);

    while (1) {
        uint8_t raw_data[14];
        esp_err_t err = mpu6050_read_bytes(MPU6050_REG_ACCEL_XOUT_H, raw_data, 14);

        if (err == ESP_OK) {
            int16_t accel_x = (raw_data[0] << 8) | raw_data[1];
            int16_t accel_y = (raw_data[2] << 8) | raw_data[3];
            int16_t accel_z = (raw_data[4] << 8) | raw_data[5];

            int16_t temp_raw = (raw_data[6] << 8) | raw_data[7];

            int16_t gyro_x = (raw_data[8] << 8) | raw_data[9];
            int16_t gyro_y = (raw_data[10] << 8) | raw_data[11];
            int16_t gyro_z = (raw_data[12] << 8) | raw_data[13];

            float temperature = (temp_raw / 340.0f) + 36.53f;

            ESP_LOGI(TAG,
                     "ACCEL: X=%d Y=%d Z=%d | GYRO: X=%d Y=%d Z=%d | TEMP=%.2f",
                     accel_x, accel_y, accel_z,
                     gyro_x, gyro_y, gyro_z,
                     temperature);
        } else {
            ESP_LOGE(TAG, "Erro ao ler MPU6050");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}