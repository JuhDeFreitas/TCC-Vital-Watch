#include "alert_manager.h"

#include "sensors/mpu6050.h"
#include "sensors/max30102.h"
#include "sensors/motion_detector.h"

#include "mqtt/payload.h"
#include "mqtt/mqtt.h"

#include "esp_log.h"

#include <math.h>
#include <string.h>

/* =========================================================
 * CONFIG
 * ========================================================= */

static const char *TAG = "ALERT_MANAGER";

/* =========================================================
 * GLOBAL STATES
 * ========================================================= */

//static alert_state_t fall_alert    = {0};
static alert_state_t hr_alert      = {0};
static alert_state_t spo2_alert    = {0};
//static alert_state_t battery_alert = {0};

static activity_state_t last_mpu_state = {0};
static max30102_data_t last_max_data  = {0};

static activity_state_t mpu_state = USER_RESTING; 

/* =========================================================
 * ALERT PAYLOAD
 * ========================================================= */

static void trigger_alert(const char *severity)
{
    char buffer[128];

    if (build_alert_payload(severity, buffer, sizeof(buffer)))
    {
        // mqtt_publish(buffer);

        ESP_LOGW(TAG, "ALERT TRIGGERED [%s]", severity);
    }
}

/* =========================================================
 * SENSOR VALIDATION
 * ========================================================= */

static bool max_data_valid(const max30102_data_t *max)
{
    if (isnan(max->heart_rate_bpm) ||
        isnan(max->spo2_percent))
    {
        return false;
    }

    if (max->heart_rate_bpm <= 0 ||
        max->spo2_percent <= 0)
    {
        return false;
    }

    return true;
}

/* =========================================================
 * FALL DETECTION
 * ========================================================= */

//static bool detect_fall(const mpu6050_data_t *mpu)
//{
//    int32_t magnitude =
//        labs(mpu->accel_x) +
//        labs(mpu->accel_y) +
//        labs(mpu->accel_z);
//
//    return magnitude > FALL_THRESHOLD;
//}

/* =========================================================
 * HR LEVEL DETECTION
 * ========================================================= */

static health_level_t detect_hr_level(const max30102_data_t *max)
{
    float hr = max->heart_rate_bpm;

    if (hr <= HR_VERY_LOW_MAX)
        return LEVEL_VERY_LOW;

    if (hr <= HR_LOW_MAX)
        return LEVEL_LOW;

    if (hr <= HR_NORMAL_MAX)
        return LEVEL_NORMAL;

    if (hr <= HR_HIGH_MAX)
        return LEVEL_HIGH;

    return LEVEL_VERY_HIGH;
}

/* =========================================================
 * SPO2 LEVEL DETECTION
 * ========================================================= */

static health_level_t detect_spo2_level(const max30102_data_t *max)
{
    float spo2 = max->spo2_percent;

    if (spo2 <= SPO2_VERY_LOW_MAX)
        return LEVEL_VERY_LOW;

    if (spo2 <= SPO2_LOW_MAX)
        return LEVEL_LOW;

    if (spo2 <= SPO2_NORMAL_MAX)
        return LEVEL_NORMAL;

    return LEVEL_VERY_HIGH;
}

/* =========================================================
 * ALERT DEBOUNCE
 * ========================================================= */

static void update_alert(
    alert_state_t *alert,
    bool condition,
    const char *severity)
{
    if (condition){
        alert->counter++;
    }
    else{
        alert->counter = 0;
        alert->active  = false;
    }

    if (alert->counter >= ALERT_DEBOUNCE_COUNT && !alert->active){
        trigger_alert(severity);

        alert->active = true;
    }
}

/* =========================================================
 * CHANGE DETECTION
 * ========================================================= */

static bool mpu_state_changed(void)
{   
    mpu_state = mpu_get_activity_state();

    if( mpu_state != last_mpu_state ){
        ESP_LOGI(TAG, "MPU STATE CHANGED: %d -> %d", last_mpu_state, mpu_state);
        return true;
    }

    return false;
}

static bool max_data_changed(void)
{
    return (
        fabs(g_max_data.heart_rate_bpm -
             last_max_data.heart_rate_bpm) > 1.0f ||

        fabs(g_max_data.spo2_percent -
             last_max_data.spo2_percent) > 1.0f
    );
}

/* =========================================================
 * MPU UPDATE
 * ========================================================= */

//static void handle_mpu_update(void)
//{
    //bool fall_detected;

    //last_mpu_state = mpu_get_activity_state();

    //fall_detected = detect_fall(&g_mpu_state);

    //ESP_LOGI(
    //    TAG,
    //    "[MPU] AX=%d AY=%d AZ=%d FALL=%d",
    //    mpu_state.accel_x,
    //    mpu_state.accel_y,
    //    mpu_state.accel_z,
    //    fall_detected
    //);

    //update_alert(
    //    &fall_alert,
    //    fall_detected,
    //    "critical"
    //);
//}

/* =========================================================
 * MAX30102 UPDATE
 * ========================================================= */

static void handle_max_update(void)
{
    health_level_t hr_level;
    health_level_t spo2_level;

    bool hr_alert_condition;
    bool spo2_alert_condition;

    last_max_data = g_max_data;

    /* Check if the data is valid */
    if (!max_data_valid(&g_max_data))
    {
        ESP_LOGW(TAG, "INVALID MAX30102 DATA");
        return;
    }

    /* Detect health levels */
    hr_level   = detect_hr_level(&g_max_data);
    spo2_level = detect_spo2_level(&g_max_data);

    /* Determine alert conditions */
    hr_alert_condition = (hr_level == LEVEL_VERY_LOW) || 
                         (hr_level == LEVEL_VERY_HIGH);

    spo2_alert_condition = (spo2_level == LEVEL_VERY_LOW);

    ESP_LOGI(
        TAG,
        "[MAX] HR=%.1f BPM | SPO2=%.1f%% | HR_LEVEL=%d | SPO2_LEVEL=%d",
        g_max_data.heart_rate_bpm,
        g_max_data.spo2_percent,
        hr_level,
        spo2_level
    );

    /* Update alerts */
    update_alert(
        &hr_alert,
        hr_alert_condition,
        "warning"
    );

    update_alert(
        &spo2_alert,
        spo2_alert_condition,
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
        //if (mpu_state_changed())
        //{
        //    handle_mpu_update();
        //}
    
        if (max_data_changed() || mpu_state_changed())
        {
            handle_max_update();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}