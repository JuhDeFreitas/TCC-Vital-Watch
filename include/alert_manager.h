#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "sensors/mpu6050.h"
#include "sensors/max30102.h"

/* =========================================================
 * TASK CONFIGURATION
 * ========================================================= */

//#define TASK_ALERT_INTERVAL_MS      2000

/* =========================================================
 * HEART RATE THRESHOLDS (BPM)
 * ========================================================= */

/* Resting state */
/*
#define HR_REST_VERY_LOW_MAX        39
#define HR_REST_LOW_MAX             59
#define HR_REST_NORMAL_MAX         100
#define HR_REST_HIGH_MAX           120
#define HR_REST_VERY_HIGH_MAX      200*/

/* Running state */
/*
#define HR_RUN_VERY_LOW_MAX         79
#define HR_RUN_LOW_MAX            119
#define HR_RUN_NORMAL_MAX         159
#define HR_RUN_HIGH_MAX           179
#define HR_RUN_VERY_HIGH_MAX      220*/

/* Values above HR_*_VERY_HIGH_MAX are classified as VERY_HIGH */

/* =========================================================
 * SPO2 THRESHOLDS (%)
 * ========================================================= */

#define SPO2_VERY_LOW_MAX           88  /* Severe hypoxemia */
#define SPO2_LOW_MAX                94  /* Attention required */
#define SPO2_NORMAL_MAX            100  /* Expected range */

/* Values above 100% may indicate sensor error */

/* =========================================================
 * ALERT CONFIGURATION
 * ========================================================= */

typedef struct
{
    float spo2_very_low;
    float spo2_low;
    float spo2_normal;

    uint32_t hr_very_low;
    uint32_t hr_low;
    uint32_t hr_normal;
    uint32_t hr_high;
    uint32_t hr_very_high;

    // float fall_threshold;

    float motion_threshold;
    float motion_min_interval_ms;
    float motion_max_interval_ms;

} alert_config_t;

extern alert_config_t alert_config;

/* =========================================================
 * DETECTION PARAMETERS
 * ========================================================= */

#define TEMP_MAX_VALUE             40.0f

#define FALL_THRESHOLD             20000

#define ALERT_DEBOUNCE_COUNT       3

#define SENSOR_TIMEOUT_SEC         5

/* =========================================================
 * TYPES
 * ========================================================= */

typedef enum
{
    LEVEL_INVALID = -1,
    LEVEL_VERY_LOW,
    LEVEL_LOW,
    LEVEL_NORMAL,
    LEVEL_HIGH,
    LEVEL_VERY_HIGH

} health_level_t;

typedef struct
{
    uint8_t counter;
    bool active;

} alert_state_t;

typedef struct
{
    max30102_data_t data;
    time_t last_update;
    bool valid;

} max_state_t;

/* =========================================================
 * PUBLIC API
 * ========================================================= */

 

/**
 * @brief Updates alert thresholds from a JSON configuration.
 *
 * @param data JSON string containing threshold parameters.
 */
void set_threshold_config(const char *data);

/**
 * @brief Loads alert thresholds from NVS into the alert_config structure.
 * Should be called during system initialization to apply saved configurations.
 */
bool load_threshold_config(void); 


/**
 * @brief Initializes the alert manager module.
 *
 * Must be called before starting the alert manager task.
 * Initializes internal states and sensor event queues.
 *
 * @param mpu_q Queue used for MPU6050 events.
 * @param max_q Queue used for MAX30102 events.
 */
void alert_manager_init(QueueHandle_t mpu_q, QueueHandle_t max_q);

/**
 * @brief Alert manager task.
 *
 * Consumes sensor events, evaluates health metrics,
 * and triggers alerts when abnormal conditions are detected.
 *
 * @param arg Task parameter (unused).
 */
void alert_task(void *arg);
void alert_task_init(void);
void alert_task_suspend(void);
void alert_task_resume(void);