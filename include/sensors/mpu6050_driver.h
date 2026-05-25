#ifndef MPU6050_DRIVER_H
#define MPU6050_DRIVER_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"

#define MPU6050_ADDR 0x68

#define MPU6050_INT_PIN GPIO_NUM_4

extern TaskHandle_t motion_task_handle;

uint8_t read_int_status(void);

void mpu6050_init(void);

void mpu6050_enable_motion_interrupt(void);

void mpu6050_read_accel_raw(
    int16_t *ax,
    int16_t *ay,
    int16_t *az
);

void mpu_gpio_interrupt_init(void);

#endif