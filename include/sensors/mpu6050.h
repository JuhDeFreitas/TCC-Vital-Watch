#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "sensors/motion_detector.h"

/* Private Variables ============================================= */

/* Endereço I2C do MPU6050 */
#define MPU6050_ADDR 0x68

/* Pino de interrupção para wake-up */
#define MPU6050_INT_PIN GPIO_NUM_4

/* Modos de atividade do usuário */
typedef enum
{
    USER_RESTING,
    USER_WALKING,
    USER_RUNNING
} activity_state_t;

//extern activity_state_t mpu_state;

/* Public Functions ============================================ */

uint8_t mpu_status(void);

activity_state_t mpu_get_activity_state(void);

void mpu_init(void);

void mpu_config_motion_interrupt(void);

void mpu_read_accel(
    int16_t *ax,
    int16_t *ay,
    int16_t *az
);

void mpu_gpio_interrupt_init(void);

void mpu_motion_task(void *arg);

 void mpu_motion_task_suspend(void);
void mpu_motion_task_resume(void);

#endif