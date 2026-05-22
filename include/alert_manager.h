#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "sensors/mpu6050.h"
#include "sensors/max30102.h"

/* ================= CONFIG ================= */

#define HR_MAX_VALUE     180
#define HR_MIN_VALUE     40

#define SPO2_MIN_VALUE   90

#define TEMP_MAX_VALUE   40.0f

#define FALL_THRESHOLD   20000

#define ALERT_DEBOUNCE_COUNT 3

#define SENSOR_TIMEOUT_SEC 5

/* ================= TYPES ================= */

typedef struct {
    uint8_t counter;
    bool active;
} alert_state_t;

typedef struct {
    mpu6050_data_t data;
    time_t last_update;
    bool valid;
} mpu_state_t;

typedef struct {
    max30102_data_t data;
    time_t last_update;
    bool valid;
} max_state_t;


/* ================= FUNÇÕES  ================= */

/**
 * @brief Inicializa o módulo de gerenciamento de alertas.
 *
 * Deve ser chamado antes de iniciar a task. Pode ser usado para
 * configurar estados internos e parâmetros de detecção.
 */
void alert_manager_init(QueueHandle_t mpu_q, QueueHandle_t max_q);

/**
 * @brief Task responsável por consumir eventos dos sensores,
 *        avaliar métricas e disparar alertas.
 *
 * @param arg Parâmetro da task (não utilizado).
 */
void alert_manager_task(void *arg);