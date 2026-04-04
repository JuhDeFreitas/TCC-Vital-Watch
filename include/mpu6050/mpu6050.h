#ifndef MPU6050_H
#define MPU6050_H

#include "driver/i2c.h"

esp_err_t mpu6050_write_byte(uint8_t reg, uint8_t data);
esp_err_t mpu6050_read_bytes(uint8_t reg, uint8_t *data, size_t len);
void start_mpu6050(void *pvParameters);

#endif // MPU6050_H
