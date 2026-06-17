#include "config_manager.h"
#include "wifi_manager.h"

#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "CFG";

/* Valores padrão das configurações */
sampling_config_t g_sampling_config = {
    .sampling_interval_ms = 1000,
    .publish_interval_ms  = 2000,
};

alert_config_t g_alert_config = {
    /* FC repouso — OMS/AHA */
    .hr_very_low            = 50,    // < 50  → muito baixa
    .hr_low                 = 60,    // < 60  → baixa  (50-59)
    .hr_high                = 101,   // >=101 → alta   (101-120)
    .hr_very_high           = 121,   // >=121 → muito alta (>120)

    /* FC corrida — OMS/AHA */
    .hr_running_very_low    = 100,   // <100  → muito baixa em corrida
    .hr_running_low         = 110,   // <110  → baixa  (100-109)
    .hr_running_high        = 150,   // >=150 → alta   (150-170)
    .hr_running_very_high   = 171,   // >=171 → muito alta (>170)

    /* SpO2 — OMS */
    .spo2_very_low          = 89,    // <=89  → muito baixa (<90%)
    .spo2_low               = 94,    // <=94  → baixa  (90-94%)
    .spo2_normal            = 100,
    .motion_threshold       = 1.5f,
    .motion_min_interval_ms = 200,
    .motion_max_interval_ms = 800,
};

/* -------------------------------------------------------------------
 * SAMPLING CONFIG
 * Payload: {"sampling_interval_ms": 1000, "publish_interval_ms": 2000}
 * ------------------------------------------------------------------- */
bool config_parse_sampling(const char *payload, int len)
{
    /* Payload MQTT não é null-terminated — copia para buffer antes do parse */
    char buf[256] = {0};
    memcpy(buf, payload, len < 255 ? len : 255);

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        ESP_LOGW(TAG, "JSON de sampling invalido");
        return false;
    }

    cJSON *sampling_interval = cJSON_GetObjectItem(json, "sampling_interval_ms");
    cJSON *publish_interval  = cJSON_GetObjectItem(json, "publish_interval_ms");

    if (!cJSON_IsNumber(sampling_interval) || !cJSON_IsNumber(publish_interval)) {
        ESP_LOGW(TAG, "Campos de sampling invalidos");
        cJSON_Delete(json);
        return false;
    }

    g_sampling_config.sampling_interval_ms = (uint32_t)sampling_interval->valueint;
    g_sampling_config.publish_interval_ms  = (uint32_t)publish_interval->valueint;

    ESP_LOGI(TAG, "Sampling: %lu ms | Publish: %lu ms",
             (unsigned long)g_sampling_config.sampling_interval_ms,
             (unsigned long)g_sampling_config.publish_interval_ms);

    cJSON_Delete(json);
    return true;
}

/* -------------------------------------------------------------------
 * WIFI CONFIG
 * Payload: {"ssid": "MinhaRede", "password": "minhaSenha"}
 * ------------------------------------------------------------------- */
bool config_parse_wifi(const char *payload, int len)
{
    char buf[256] = {0};
    memcpy(buf, payload, len < 255 ? len : 255);

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        ESP_LOGW(TAG, "JSON de WiFi invalido");
        return false;
    }

    cJSON *ssid     = cJSON_GetObjectItem(json, "ssid");
    cJSON *password = cJSON_GetObjectItem(json, "password");

    if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
        ESP_LOGW(TAG, "Campos de WiFi invalidos");
        cJSON_Delete(json);
        return false;
    }

    char ssid_buf[33]     = {0};
    char password_buf[65] = {0};
    strlcpy(ssid_buf,     ssid->valuestring,     sizeof(ssid_buf));
    strlcpy(password_buf, password->valuestring, sizeof(password_buf));

    ESP_LOGI(TAG, "WiFi config recebida: SSID=%s", ssid_buf);

    cJSON_Delete(json);

    wifi_manager_connect(ssid_buf, password_buf);
    return true;
}

/* -------------------------------------------------------------------
 * THRESHOLD CONFIG
 * Payload (todos os campos são opcionais):
 * {
 *   "hr_very_low": 50, "hr_low": 60, "hr_high": 101, "hr_very_high": 121,
 *   "hr_high": 130,     "hr_very_high": 160,
 *   "spo2_very_low": 90, "spo2_low": 94, "spo2_normal": 100,
 *   "motion_threshold": 1.5,
 *   "motion_min_interval_ms": 200, "motion_max_interval_ms": 800
 * }
 * ------------------------------------------------------------------- */
bool config_parse_thresholds(const char *payload, int len)
{
    char buf[512] = {0};
    memcpy(buf, payload, len < 511 ? len : 511);

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        ESP_LOGW(TAG, "JSON de thresholds invalido");
        return false;
    }

    cJSON *item;

    item = cJSON_GetObjectItem(json, "hr_very_low");
    if (cJSON_IsNumber(item)) g_alert_config.hr_very_low = (uint8_t)item->valueint;

    item = cJSON_GetObjectItem(json, "hr_low");
    if (cJSON_IsNumber(item)) g_alert_config.hr_low = (uint8_t)item->valueint;


    item = cJSON_GetObjectItem(json, "hr_high");
    if (cJSON_IsNumber(item)) g_alert_config.hr_high = (uint8_t)item->valueint;

    item = cJSON_GetObjectItem(json, "hr_very_high");
    if (cJSON_IsNumber(item)) g_alert_config.hr_very_high = (uint8_t)item->valueint;

    item = cJSON_GetObjectItem(json, "hr_running_very_low");
    if (cJSON_IsNumber(item)) g_alert_config.hr_running_very_low = (uint8_t)item->valueint;

    item = cJSON_GetObjectItem(json, "hr_running_low");
    if (cJSON_IsNumber(item)) g_alert_config.hr_running_low = (uint8_t)item->valueint;

    item = cJSON_GetObjectItem(json, "hr_running_high");
    if (cJSON_IsNumber(item)) g_alert_config.hr_running_high = (uint8_t)item->valueint;

    item = cJSON_GetObjectItem(json, "hr_running_very_high");
    if (cJSON_IsNumber(item)) g_alert_config.hr_running_very_high = (uint8_t)item->valueint;

    item = cJSON_GetObjectItem(json, "spo2_very_low");
    if (cJSON_IsNumber(item)) g_alert_config.spo2_very_low = (uint8_t)item->valueint;

    item = cJSON_GetObjectItem(json, "spo2_low");
    if (cJSON_IsNumber(item)) g_alert_config.spo2_low = (uint8_t)item->valueint;

    item = cJSON_GetObjectItem(json, "spo2_normal");
    if (cJSON_IsNumber(item)) g_alert_config.spo2_normal = (uint8_t)item->valueint;

    item = cJSON_GetObjectItem(json, "motion_threshold");
    if (cJSON_IsNumber(item)) g_alert_config.motion_threshold = (float)item->valuedouble;

    item = cJSON_GetObjectItem(json, "motion_min_interval_ms");
    if (cJSON_IsNumber(item)) g_alert_config.motion_min_interval_ms = (uint32_t)item->valueint;

    item = cJSON_GetObjectItem(json, "motion_max_interval_ms");
    if (cJSON_IsNumber(item)) g_alert_config.motion_max_interval_ms = (uint32_t)item->valueint;

    ESP_LOGI(TAG, "Thresholds atualizados");

    cJSON_Delete(json);
    return true;
}
