/**
 * @file payload.c
 * @brief Builds JSON payloads for MQTT communication.
 *
 * This module provides helper functions to create JSON messages
 * containing biometric measurements and system alerts.
 * 
 */

#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_log.h"
#include "mqtt/payload.h"
#include "device_info.h"
#include "wifi.h"
#include "alert_manager.h"
#include "sensors/max30102.h"

bool build_max30102_payload(const max30102_data_t *metrics, char *buffer, size_t buffer_size)
{
    if (metrics == NULL || buffer == NULL){
        return false;
    }

    cJSON *root = cJSON_CreateObject();

    if (root == NULL){
        return false;
    }

    time_t now;

    time(&now);

    cJSON_AddNumberToObject(root, "timestamp", (double)now);
    cJSON_AddNumberToObject(root, "heart_rate_bpm", metrics->heart_rate_bpm);
    cJSON_AddNumberToObject(root, "spo2_percent", metrics->spo2_percent);

    bool success = cJSON_PrintPreallocated(root, buffer, buffer_size, false);

    cJSON_Delete(root);

    return success;
}


static const char *TAG = "PAYLOAD";

bool build_alert_payload(
                        const char *type, 
                        const char *severity, 
                        char *buffer, 
                        size_t buffer_size)
{
    if (type == NULL || severity == NULL || buffer == NULL){
        return false;
    }

    cJSON *root = cJSON_CreateObject();

    if (root == NULL){
        return false;
    }

    time_t now;
    time(&now);

    cJSON_AddStringToObject(root, "type", type);
    cJSON_AddStringToObject(root, "severity", severity);
    cJSON_AddNumberToObject(root, "timestamp", (double)now);

    bool success = cJSON_PrintPreallocated(root, buffer, buffer_size, false);

    cJSON_Delete(root);

    return success;
}

/** ========================================================
 * PARSE JSON 
 * ========================================================= */


 /**
 * @brief Parse sampling configuration JSON payload.
 *
 * Expected payload:
 *
 * {
 *   "sampling_interval_ms": 1000,
 *   "publish_interval_ms": 2000
 * }
 *
 * @param data JSON string received from MQTT.
 *
 * @return true  Configuration parsed successfully.
 * @return false Invalid JSON or invalid fields.
 */
bool parse_sampling_config(const char *data)
{
    /* Parse JSON payload */
    cJSON *json = cJSON_Parse(data);

    if (json == NULL){
        ESP_LOGW(TAG, "Failed to parse sampling config JSON");
        return false;
    }

    /* Get JSON fields */
    cJSON *sampling_interval = cJSON_GetObjectItem(json, "sampling_interval_ms");

    cJSON *publish_interval = cJSON_GetObjectItem(json, "publish_interval_ms");

    /* Validate JSON fields */
    if (!cJSON_IsNumber(sampling_interval) ||
        !cJSON_IsNumber(publish_interval))
    {
        ESP_LOGW(TAG, "Invalid sampling config fields");

        cJSON_Delete(json);

        return false;
    }

    /* Update configuration structure */
    g_sampling_config.sampling_interval_ms = (uint32_t)sampling_interval->valueint;

    g_sampling_config.publish_interval_ms = (uint32_t)publish_interval->valueint;

    ESP_LOGI(TAG,
             "Sampling interval: %lu ms",
             g_sampling_config.sampling_interval_ms);

    ESP_LOGI(TAG,
             "Publish interval: %lu ms",
             g_sampling_config.publish_interval_ms);

    /* Free JSON object */
    cJSON_Delete(json);

    return true;
}

bool parse_wifi_config(const char *data)
{
    cJSON *json = cJSON_Parse(data);

    if (json == NULL) {
        ESP_LOGW(TAG, "Failed to parse Wi-Fi config JSON");
        return false;
    }

    cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
    cJSON *password = cJSON_GetObjectItem(json, "password");

    if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
        ESP_LOGW(TAG, "Invalid Wi-Fi config fields");
        cJSON_Delete(json);
        return false;
    }

    strncpy(g_wifi_config.ssid, ssid->valuestring, sizeof(g_wifi_config.ssid) - 1);
    strncpy(g_wifi_config.password, password->valuestring, sizeof(g_wifi_config.password) - 1);

    ESP_LOGI(TAG, "Wi-Fi config updated: SSID=%s", g_wifi_config.ssid);

    cJSON_Delete(json);

    wifi_reconnect();
    return true;    
}

bool parse_threshold_config(const char *data)
{
    cJSON *json = cJSON_Parse(data);
    if (json == NULL) {
        ESP_LOGW(TAG, "Failed to parse threshold config JSON");
        return false;
    }

    bool updated = false;

    cJSON *item;

    item = cJSON_GetObjectItem(json, "hr_very_low");
    if (cJSON_IsNumber(item)) {
        alert_config.hr_very_low = (uint8_t)item->valueint;
        updated = true;
    }

    item = cJSON_GetObjectItem(json, "hr_low");
    if (cJSON_IsNumber(item)) {
        alert_config.hr_low = (uint8_t)item->valueint;
        updated = true;
    }

    item = cJSON_GetObjectItem(json, "hr_normal");
    if (cJSON_IsNumber(item)) {
        alert_config.hr_normal = (uint8_t)item->valueint;
        updated = true;
    }

    item = cJSON_GetObjectItem(json, "hr_high");
    if (cJSON_IsNumber(item)) {
        alert_config.hr_high = (uint8_t)item->valueint;
        updated = true;
    }

    item = cJSON_GetObjectItem(json, "hr_very_high");
    if (cJSON_IsNumber(item)) {
        alert_config.hr_very_high = (uint8_t)item->valueint;
        updated = true;
    }

    item = cJSON_GetObjectItem(json, "spo2_very_low");
    if (cJSON_IsNumber(item)) {
        alert_config.spo2_very_low = (uint8_t)item->valueint;
        updated = true;
    }

    item = cJSON_GetObjectItem(json, "spo2_low");
    if (cJSON_IsNumber(item)) {
        alert_config.spo2_low = (uint8_t)item->valueint;
        updated = true;
    }

    item = cJSON_GetObjectItem(json, "spo2_normal");
    if (cJSON_IsNumber(item)) {
        alert_config.spo2_normal = (uint8_t)item->valueint;
        updated = true;
    }

    item = cJSON_GetObjectItem(json, "motion_threshold");
    if (cJSON_IsNumber(item)) {
        alert_config.motion_threshold = (float)item->valuedouble;
        updated = true;
    }

    item = cJSON_GetObjectItem(json, "motion_min_interval_ms");
    if (cJSON_IsNumber(item)) {
        alert_config.motion_min_interval_ms = (uint32_t)item->valueint;
        updated = true;
    }

    item = cJSON_GetObjectItem(json, "motion_max_interval_ms");
    if (cJSON_IsNumber(item)) {
        alert_config.motion_max_interval_ms = (uint32_t)item->valueint;
        updated = true;
    }

    if (!updated) {
        ESP_LOGW(TAG, "No valid threshold fields found in JSON");
    }

    cJSON_Delete(json);
    return updated;
}
