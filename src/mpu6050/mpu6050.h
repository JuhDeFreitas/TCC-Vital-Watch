#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include "esp_err.h"

// ─── I2C ────────────────────────────────────────────────────────────────────
#define MPU6050_ADDR            0x68    // AD0 = GND  (use 0x69 se AD0 = VCC)

// ─── GPIO conectado ao pino INT do MPU6050 ──────────────────────────────────
#define MPU6050_INT_GPIO        4

// ─── Registradores ──────────────────────────────────────────────────────────
#define MPU_REG_SMPLRT_DIV      0x19
#define MPU_REG_CONFIG          0x1A
#define MPU_REG_ACCEL_CONFIG    0x1C
#define MPU_REG_INT_PIN_CFG     0x37
#define MPU_REG_INT_ENABLE      0x38
#define MPU_REG_INT_STATUS      0x3A
#define MPU_REG_ACCEL_XOUT_H    0x3B
#define MPU_REG_PWR_MGMT_1      0x6B
#define MPU_REG_WHO_AM_I        0x75

// ─── Parâmetros de detecção de corrida ──────────────────────────────────────
//
// Com ±2g: 1 g = 16384 LSB
//
// Sensor no pulso — detecção pelo eixo X dinâmico (balanço do braço).
// A gravidade é removida via filtro EMA, então os limiares representam
// amplitude de oscilação residual, não aceleração absoluta.
//
// STEP_HIGH_G_LSB — pico dinâmico em X para contar como candidato a passo.
//   3000 ≈ 0.18 g dinâmico. Suficiente para balanço de corrida, descarta
//   movimentos lentos de pulso e tremores.
// STEP_LOW_G_LSB  — histerese: dinâmica deve cair abaixo antes do próximo pico.
// RUN_MIN/MAX_INTERVAL_MS — janela de cadência: 92–272 spm.
// RUN_CONFIRM_STEPS — passos consecutivos para confirmar corrida.
// RUN_RHYTHM_TOL_PCT — tolerância de variação de ritmo em %.
// RUN_TIMEOUT_MS — sem passo válido por este tempo → MPU_EVENT_STOPPED.

#define STEP_HIGH_G_LSB         3000
#define STEP_LOW_G_LSB          1500
#define RUN_MIN_INTERVAL_MS     220
#define RUN_MAX_INTERVAL_MS     650
#define RUN_CONFIRM_STEPS       5
#define RUN_RHYTHM_TOL_PCT      40
#define RUN_TIMEOUT_MS          5000

// ─── Eventos ────────────────────────────────────────────────────────────────

typedef enum {
    MPU_EVENT_RUNNING,  // corrida confirmada; cadence_spm é válido
    MPU_EVENT_STOPPED,  // parou de correr; cadence_spm = 0
} mpu6050_event_t;

// ─── API ────────────────────────────────────────────────────────────────────

// Callback chamado no contexto da task do MPU.
// event=RUNNING → passo confirmado durante corrida, cadence_spm = passos/min.
// event=STOPPED → corrida encerrada.
typedef void (*mpu6050_motion_cb_t)(mpu6050_event_t event, uint32_t cadence_spm);

esp_err_t mpu6050_init(mpu6050_motion_cb_t on_motion);
uint8_t   mpu6050_who_am_i(void);

#endif
