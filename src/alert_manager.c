#include "alert_manager.h"

#include "sensors/mpu6050.h"
#include "sensors/max30102.h"
#include "sensors/motion_detector.h"

#include "mqtt/payload.h"
#include "mqtt/mqtt.h"
#include "device_info.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_log.h"

#include <math.h>
#include <string.h>

/* =========================================================
 *  Definitions
 * ========================================================= */

static const char *TAG = "ALERT_MANAGER";

//static alert_state_t fall_alert    = {0};
static alert_state_t hr_alert      = {0};
static alert_state_t spo2_alert    = {0};
//static alert_state_t battery_alert = {0};

static activity_state_t last_mpu_state = USER_RESTING;
static activity_state_t mpu_state = USER_RESTING; 
static max30102_data_t last_max_data  = {0};

TaskHandle_t alert_task_handle = NULL;


alert_config_t alert_config = {
    .hr_very_low = 39,
    .hr_low      = 59,
    .hr_normal   = 100,
    .hr_high     = 120,
    .hr_very_high= 200,

    .spo2_very_low = 88,
    .spo2_low    = 94,
    .spo2_normal = 100,

    //.fall_threshold = 2.5f,

    .motion_threshold = 0.35f,
    .motion_min_interval_ms = 180,
    .motion_max_interval_ms = 500
};

/* =========================================================
 * CONFIG ALERT PROPERTIES
 * ========================================================= */

 #define NVS_NAMESPACE "alert_config"
 #define NVS_KEY_THRESHOLDS "thresholds"

void set_threshold_config(const char *data)
{   
    // Parse received JSON into alert_config (configuration structure)
    parse_threshold_config(data);

    // Save the new config to NVS
    nvs_handle_t nvs_handle;

    esp_err_t err = nvs_open(
        NVS_NAMESPACE,
        NVS_READWRITE,
        &nvs_handle
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS");
        return;
    }

    err = nvs_set_blob(
        nvs_handle,
        NVS_KEY_THRESHOLDS,
        &alert_config,
        sizeof(alert_config)
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save threshold config");
        nvs_close(nvs_handle);
        return;
    }

    nvs_commit(nvs_handle);

    ESP_LOGI(TAG, "Threshold config saved");
}


bool load_threshold_config(void)
{
    nvs_handle_t nvs_handle;

    size_t required_size = sizeof(alert_config);

    esp_err_t err = nvs_open(
        NVS_NAMESPACE,
        NVS_READWRITE,
        &nvs_handle
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS");
        return false;
    }

    err = nvs_get_blob(
        nvs_handle,
        NVS_KEY_THRESHOLDS,
        &alert_config,
        &required_size
    );

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Threshold config loaded");
        ESP_LOGI(TAG,
          "HR thresholds: very_low=%u, low=%u, normal=%u, high=%u, very_high=%u",
            (unsigned int)alert_config.hr_very_low,
            (unsigned int)alert_config.hr_low,
            (unsigned int)alert_config.hr_normal,
            (unsigned int)alert_config.hr_high,
            (unsigned int)alert_config.hr_very_high);
        ESP_LOGI(TAG,
          "SpO2 thresholds: very_low=%.1f, low=%.1f, normal=%.1f",
            alert_config.spo2_very_low,
            alert_config.spo2_low,
            alert_config.spo2_normal);
        ESP_LOGI(TAG,
          "Motion threshold: %.2f, min_interval=%.0f ms, max_interval=%.0f ms",
            alert_config.motion_threshold,
            alert_config.motion_min_interval_ms,
            alert_config.motion_max_interval_ms);

        return true;
    }

    ESP_LOGW(TAG, "Threshold config not found. Using defaults.");

    nvs_close(nvs_handle);
    return false;
}


/* =========================================================
 * PRIVATE FUNCTIONS
 * ========================================================= */

 /**
 * @brief Publishes an alert message through MQTT.
 *
 * Builds an alert payload and sends it to the alert topic.
 *
 * @param type Alert type.
 * @param severity Alert severity level.
 */
static void trigger_alert(const char *type, const char *severity)
{
    char buffer[128];

    if (build_alert_payload(type, severity, buffer, sizeof(buffer)))
    {
        mqtt_publish_message(TOPIC_ALERTS, buffer);

        ESP_LOGW(TAG, "ALERT TRIGGERED [%s][%s]", severity, type);
    }
}

/**
 * @brief Validates MAX30102 measurements.
 *
 * Checks if heart rate and SpO2 values are valid
 * and within an acceptable range.
 *
 * @param max Pointer to sensor data.
 *
 * @return true if data is valid.
 * @return false otherwise.
 */
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
 * LEVEL DETECTION
 * ========================================================= */

/**
 * @brief Determines the heart rate health level.
 *
 * Heart rate thresholds are selected according to
 * the current user activity state.
 *
 * @param max Pointer to sensor data.
 *
 * @return Detected health level.
 */
static health_level_t detect_hr_level(const max30102_data_t *max)
{
    float hr = max->heart_rate_bpm;

    int very_low_max;
    int low_max;
    int normal_max;
    int high_max;

    switch(mpu_state){
        case USER_RESTING:
            very_low_max = alert_config.hr_very_low;
            low_max      = alert_config.hr_low;
            normal_max   = alert_config.hr_normal;
            high_max     = alert_config.hr_high;
            break;

        //case USER_WALKING:
        //    very_low_max = HR_WALK_VERY_LOW_MAX;
        //    low_max      = HR_WALK_LOW_MAX;
        //    normal_max   = HR_WALK_NORMAL_MAX;
        //    high_max     = HR_WALK_HIGH_MAX;
        //    break;

        case USER_RUNNING:
            very_low_max = alert_config.hr_very_low;
            low_max      = alert_config.hr_low;
            normal_max   = alert_config.hr_normal;
            high_max     = alert_config.hr_high;
            break;

        default:

            return LEVEL_INVALID;
    }

    if (hr <= very_low_max)
        return LEVEL_VERY_LOW;

    if (hr <= low_max)
        return LEVEL_LOW;

    if (hr <= normal_max)
        return LEVEL_NORMAL;

    if (hr <= high_max)
        return LEVEL_HIGH;

    return LEVEL_VERY_HIGH;
}


/**
 * @brief Determines the SpO2 health level.
 *
 * @param max Pointer to sensor data.
 *
 * @return Detected health level.
 */
static health_level_t detect_spo2_level(const max30102_data_t *max)
{
    float spo2 = max->spo2_percent;

    if (spo2 <= SPO2_VERY_LOW_MAX)
        return LEVEL_VERY_LOW;

    if (spo2 <= SPO2_LOW_MAX)
        return LEVEL_LOW;

    if (spo2 <= SPO2_NORMAL_MAX)
        return LEVEL_NORMAL;

    return LEVEL_INVALID;
}


/**
 * @brief Converts a health level to a string.
 *
 * @param level Health level.
 *
 * @return String representation of the level.
 */
static const char *health_level_to_string(health_level_t level)
{
    switch(level)
    {
        case LEVEL_VERY_LOW:  return "very_low";
        case LEVEL_LOW:       return "low";
        case LEVEL_NORMAL:    return "normal";
        case LEVEL_HIGH:      return "high";
        case LEVEL_VERY_HIGH: return "very_high";

        default:              return "unknown";
    }
}


/* =========================================================
 * ALERT DEBOUNCE
 * ========================================================= */

 /**
 * @brief Updates alert state using debounce validation.
 *
 * Triggers the alert only if the condition remains
 * active for a defined number of consecutive checks.
 * 
 * @param alert Alert state structure.
 * @param condition Alert condition.
 * @param type Alert type.
 * @param severity Alert severity.
 */
static void update_alert(alert_state_t *alert, 
                         bool condition, 
                         const char *type,
                         const char *severity)
{
    TickType_t now = xTaskGetTickCount();

    if (condition)
    {
        alert->counter++;

        // Primeiro disparo (com debounce)
        if (alert->counter >= ALERT_DEBOUNCE_COUNT && !alert->active)
        {
            trigger_alert(type, severity);
            alert->active = true;
            alert->last_sent = now;
        }

        // REENVIO CONTÍNUO
        else if (alert->active &&
                 (now - alert->last_sent) >= g_sampling_config.publish_interval_ms)
        {
            trigger_alert(type, severity);
            alert->last_sent = now;
        }
    }
    else
    {
        // Reset quando volta ao normal
        alert->counter = 0;
        alert->active  = false;
    }
}

/* =========================================================
 * CHANGE DETECTION
 * ========================================================= */

 /**
 * @brief Detects changes in user activity state.
 *
 * @return true if the state changed.
 * @return false otherwise.
 */
static bool mpu_state_changed(void)
{   
    mpu_state = mpu_get_activity_state();

    if( mpu_state != last_mpu_state ){
        ESP_LOGI(TAG, "MPU STATE CHANGED: %d -> %d", last_mpu_state, mpu_state);
        last_mpu_state = mpu_state;
        return true;
    }

    return false;
}


/**
 * @brief Detects significant changes in biometric data.
 *
 * @return true if data changed.
 * @return false otherwise.
 */
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
 * MAX30102 UPDATE HANDLER
 * ========================================================= */

/**
 * @brief Processes MAX30102 measurements.
 *
 * Validates sensor data, evaluates health levels
 * and updates alert states.
 */
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
    hr_alert_condition = (hr_level == LEVEL_VERY_LOW)   || 
                         (hr_level == LEVEL_LOW)        ||
                         (hr_level == LEVEL_HIGH)       ||
                         (hr_level == LEVEL_VERY_HIGH);

    spo2_alert_condition =  (spo2_level == LEVEL_VERY_LOW) ||
                            (spo2_level == LEVEL_LOW);

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
        "heart_rate",
        health_level_to_string(hr_level)
    );

    update_alert(
        &spo2_alert,
        spo2_alert_condition,
        "spo2",
        health_level_to_string(spo2_level)
    );
}

/* =========================================================
 * MAIN TASK
 * ========================================================= */

void alert_task(void *arg)
{   
    /* Load threshold configurations */
    load_threshold_config();
    
    /* Main task loop */
    while (1)
    {    
        /* MAX30102 UPDATE */
        handle_max_update();

        /* BATTERY UPDATE */
        /* ... */

        vTaskDelay(pdMS_TO_TICKS( g_sampling_config.sampling_interval_ms));
    }
}

void alert_task_init(){

    xTaskCreate(
        alert_task,
        "alert_manager_task",
        4096,
        NULL,
        5,
        &alert_task_handle
    );

    alert_task_suspend();
}

void alert_task_suspend(void)
{
    if (alert_task_handle)
    {
        vTaskSuspend(alert_task_handle);
    }
}

void alert_task_resume(void)
{
    if (alert_task_handle)
    {
        vTaskResume(alert_task_handle);
    }
}




