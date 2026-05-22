#include "alert_manager.h"

#include "sensors/mpu6050.h"
#include "sensors/max30102.h"

#include "mqtt/payload.h"
#include "mqtt/mqtt.h"

#include "esp_log.h"

#include <math.h>
#include <string.h>
#include <time.h>

/* =========================================================
 * CONFIG
 * ========================================================= */

static const char *TAG = "ALERT_MANAGER";

/* =========================================================
 * GLOBAL STATES
 * ========================================================= */

static alert_state_t fall_alert = {0};
static alert_state_t hr_alert   = {0};
static alert_state_t spo2_alert = {0};

/* Últimos valores processados
 * usados para detectar mudança
 */
static mpu6050_data_t last_mpu_data = {0};
static max30102_data_t last_max_data = {0};

/* =========================================================
 * ALERT PAYLOAD
 * ========================================================= */

static void trigger_alert(const char *severity)
{
    char buffer[128];

    if (build_alert_payload(severity, buffer, sizeof(buffer)))
    {
        // mqtt_publish(buffer);

        ESP_LOGW(TAG, "ALERTA DISPARADO [%s]", severity);
    }
}

/* =========================================================
 * DETECÇÕES
 * ========================================================= */

static bool detect_fall(const mpu6050_data_t *mpu)
{
    int32_t magnitude =
        abs(mpu->accel_x) +
        abs(mpu->accel_y) +
        abs(mpu->accel_z);

    return magnitude > FALL_THRESHOLD;
}

static bool detect_hr(const max30102_data_t *max)
{
    return (
        max->heart_rate_bpm < HR_MIN_VALUE ||
        max->heart_rate_bpm > HR_MAX_VALUE
    );
}

static bool detect_spo2(const max30102_data_t *max)
{
    return (
        max->spo2_percent < SPO2_MIN_VALUE
    );
}

/* =========================================================
 * DEBOUNCE
 * ========================================================= */

static void update_alert(
    alert_state_t *alert,
    bool condition,
    const char *severity
)
{
    if (condition)
    {
        alert->counter++;
    }
    else
    {
        alert->counter = 0;
        alert->active = false;
    }

    if (
        alert->counter >= ALERT_DEBOUNCE_COUNT &&
        !alert->active
    )
    {
        trigger_alert(severity);

        alert->active = true;
    }
}

/* =========================================================
 * CHANGE DETECTION
 * ========================================================= */

static bool mpu_data_changed(void)
{
    return memcmp(
        &g_mpu_data,
        &last_mpu_data,
        sizeof(mpu6050_data_t)
    ) != 0;
}

static bool max_data_changed(void)
{
    return memcmp(
        &g_max_data,
        &last_max_data,
        sizeof(max30102_data_t)
    ) != 0;
}

/* =========================================================
 * MPU HEALTH CHECK
 * ========================================================= */

static void handle_mpu_update(void)
{
    bool fall_detected;

    /* salva snapshot */
    last_mpu_data = g_mpu_data;

    ESP_LOGI(
        TAG,
        "[MPU] AX=%d AY=%d AZ=%d",
        g_mpu_data.accel_x,
        g_mpu_data.accel_y,
        g_mpu_data.accel_z
    );

    fall_detected = detect_fall(&g_mpu_data);

    update_alert(
        &fall_alert,
        fall_detected,
        "critical"
    );
}

/* =========================================================
 * MAX30102 HEALTH CHECK
 * ========================================================= */

static void handle_max_update(void)
{
    bool hr_detected;
    bool spo2_detected;

    /* salva snapshot */
    last_max_data = g_max_data;

    ESP_LOGI(
        TAG,
        "[MAX] HR=%.1f SpO2=%.1f",
        g_max_data.heart_rate_bpm,
        g_max_data.spo2_percent
    );

    hr_detected = detect_hr(&g_max_data);

    spo2_detected = detect_spo2(&g_max_data);

    update_alert(
        &hr_alert,
        hr_detected,
        "warning"
    );

    update_alert(
        &spo2_alert,
        spo2_detected,
        "critical"
    );
}

/* =========================================================
 * MAIN TASK
 * ========================================================= */

void alert_manager_task(void *arg)
{
    while (1)
    {
        /* =====================================
         * MPU UPDATE HANDLER
         * ===================================== */

        if (mpu_data_changed())
        {
            handle_mpu_update();
        }

        /* =====================================
         * MAX UPDATE HANDLER
         * ===================================== */

        if (max_data_changed())
        {
            handle_max_update();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}