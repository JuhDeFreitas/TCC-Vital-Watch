#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "sensors/mpu6050.h"
#include "sensors/max30102.h"

/* ================= CONFIG ================= */

#define TASK_ALERT_INTERVAL_MS        2000

// ======================
// HEART RATE (BPM)
// ======================

#define HR_REST_VERY_LOW_MAX     40
#define HR_REST_LOW_MAX          60
#define HR_REST_NORMAL_MAX      100
#define HR_REST_HIGH_MAX        140

#define HR_RUN_VERY_LOW_MAX      70
#define HR_RUN_LOW_MAX          100
#define HR_RUN_NORMAL_MAX       170
#define HR_RUN_HIGH_MAX         190

// above this = VERY HIGH

// ======================
// SPO2 (%)
// ======================

#define SPO2_VERY_LOW_MAX  88       // < 88 - hipoxemia
#define SPO2_LOW_MAX       94       // < 94 - atenção
#define SPO2_NORMAL_MAX    100      // > 90 - esperado
// above 100 can be treated as sensor error


#define TEMP_MAX_VALUE   40.0f

#define FALL_THRESHOLD   20000

#define ALERT_DEBOUNCE_COUNT 3

#define SENSOR_TIMEOUT_SEC 5

/* ================= TYPES ================= */

typedef enum
{
    LEVEL_INVALID = -1,
    LEVEL_VERY_LOW,
    LEVEL_LOW,
    LEVEL_NORMAL,
    LEVEL_HIGH,
    LEVEL_VERY_HIGH
} health_level_t;

typedef struct {
    uint8_t counter;
    bool active;
} alert_state_t;

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