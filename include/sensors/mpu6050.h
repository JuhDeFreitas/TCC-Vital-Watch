#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include <stddef.h>
#include "driver/i2c.h"

/* MPU6050 register addresses */
#define MPU6050_ADDR               0x68 
#define MPU6050_WHO_AM_I_EXPECTED  0x68
#define MPU6050_REG_WHO_AM_I       0x75
#define MPU6050_REG_PWR_MGMT_1     0x6B
#define MPU6050_REG_ACCEL_XOUT_H   0x3B

/* Estrutura para armazenar os dados lidos do MPU6050 */
typedef struct {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
    float temperature;
} mpu6050_data_t;

extern mpu6050_data_t g_mpu_data;

/** Function prototypes  ================================================= */

/** \brief Write a byte to a register
 * 
 */
esp_err_t mpu6050_write_byte(uint8_t reg, uint8_t data);

/** \brief Read bytes from a register 
 * 
*/
esp_err_t mpu6050_read_bytes(uint8_t reg, uint8_t *data, size_t len);

/** \brief Start the MPU6050 task
 * 
 */
void mpu6050_task(void *pvParameters);

#endif // MPU6050_H
