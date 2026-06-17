#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include "esp_err.h"

// ─── I2C ────────────────────────────────────────────────────────────────────
#define MPU6050_ADDR            0x68    // AD0 = GND  (use 0x69 se AD0 = VCC)

// ─── GPIO conectado ao pino INT do MPU6050 ──────────────────────────────────
#define MPU6050_INT_GPIO        4       // altere para o GPIO real do seu hardware

// ─── Registradores ──────────────────────────────────────────────────────────
#define MPU_REG_SMPLRT_DIV      0x19
#define MPU_REG_CONFIG          0x1A
#define MPU_REG_ACCEL_CONFIG    0x1C
#define MPU_REG_INT_PIN_CFG     0x37
#define MPU_REG_INT_ENABLE      0x38
#define MPU_REG_INT_STATUS      0x3A
#define MPU_REG_ACCEL_XOUT_H    0x3B   // seguido de Y (0x3D) e Z (0x3F) — lê 6 bytes
#define MPU_REG_PWR_MGMT_1      0x6B
#define MPU_REG_WHO_AM_I        0x75

// ─── Parâmetros de detecção de passo/corrida ────────────────────────────────
// Com ±2g: 1 g = 16384 LSB
//
// STEP_HIGH_G_LSB — limiar de SUBIDA: magnitude precisa superar este valor para
//   registrar um passo. Aumente para detectar só corrida; diminua para caminhada rápida.
//     18000 ≈ 1.10g  detecta caminhada rápida + corrida
//     20000 ≈ 1.22g  somente corrida / trote   ← padrão
//     23000 ≈ 1.40g  só corrida intensa / pulos
//
// STEP_LOW_G_LSB — limiar de DESCIDA (histerese): a magnitude deve cair abaixo
//   deste valor antes que o próximo passo possa ser contado. Evita duplo disparo.
//
// STEP_DEBOUNCE_MS — tempo mínimo entre dois passos detectados.
//   250 ms = máximo 240 passos/min (sprint veloz). Aumente se quiser menos disparo.

#define STEP_HIGH_G_LSB         20000
#define STEP_LOW_G_LSB          17000
#define STEP_DEBOUNCE_MS        250

// ─── API ────────────────────────────────────────────────────────────────────

// Callback invocado uma vez por passo detectado, no contexto da task do MPU.
// O parâmetro `cadence_spm` é a cadência instantânea em passos/min
// calculada a partir do intervalo entre os dois últimos passos.
typedef void (*mpu6050_motion_cb_t)(uint32_t cadence_spm);

// Inicializa sensor, configura interrupção de dados prontos (50 Hz)
// e inicia a task de detecção de corrida.
esp_err_t mpu6050_init(mpu6050_motion_cb_t on_step);

// Lê WHO_AM_I — deve retornar 0x68. Útil para verificar o barramento.
uint8_t   mpu6050_who_am_i(void);

#endif
