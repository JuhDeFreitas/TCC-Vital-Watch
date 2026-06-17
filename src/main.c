#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#include "i2c.h"
#include "max30102_api.h"
#include "mpu6050.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "device_controller.h"

static const char *TAG = "MAIN";

/* ── MQTT: handlers dos tópicos subscritos ─────────────────────────────────── */

// Compara payload (não null-terminated) com string literal de forma segura
#define PAYLOAD_IS(p, len, s)  ((len) == (int)sizeof(s)-1 && strncmp((p), (s), (len)) == 0)

static void on_cmd_command(const char *payload, int len)
{
    ESP_LOGI(TAG, "COMMAND: %.*s", len, payload);

    if      (PAYLOAD_IS(payload, len, "START"))  device_set_state(DEVICE_START);
    else if (PAYLOAD_IS(payload, len, "STOP"))   device_set_state(DEVICE_STOP);
    else if (PAYLOAD_IS(payload, len, "REBOOT")) device_set_state(DEVICE_REBOOT);
    else    ESP_LOGW(TAG, "Comando desconhecido: %.*s", len, payload);
}

static void on_cmd_config(const char *payload, int len)
{
    ESP_LOGI(TAG, "CONFIG: %.*s", len, payload);
    // payload esperado: JSON genérico de configuração
}

static void on_cmd_config_wifi(const char *payload, int len)
{
    ESP_LOGI(TAG, "CONFIG/WIFI: %.*s", len, payload);
    // payload esperado: {"ssid":"nome","password":"senha"}
}

static void on_cmd_config_sampling(const char *payload, int len)
{
    ESP_LOGI(TAG, "CONFIG/SAMPLING: %.*s", len, payload);
    // payload esperado: {"interval_ms":1000}
}

static void on_cmd_config_thresholds(const char *payload, int len)
{
    ESP_LOGI(TAG, "CONFIG/THRESHOLDS: %.*s", len, payload);
    // payload esperado: {"bpm_min":50,"bpm_max":120,"spo2_min":94}
}

/* PRIVATE FUNCTIONS ------------------------------------------------------------- */

static void on_motion(mpu6050_event_t event, uint32_t cadence_spm)
{
    if (event == MPU_EVENT_RUNNING) {
        ESP_LOGI("MPU6050", "Correndo: %lu spm", (unsigned long)cadence_spm);
    } else {
        ESP_LOGI("MPU6050", "Parou de correr");
    }
}

static void on_vitals(int bpm, double spo2)
{
    if (bpm < 0 || spo2 < 0.0) {
        ESP_LOGW(TAG, "Sinal fraco ou dedo ausente (bpm=%d, spo2=%.1f)", bpm, spo2);
        return;
    }

    ESP_LOGI(TAG, "BPM: %d | SpO2: %.1f%%", bpm, spo2);

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"bpm\":%d,\"spo2\":%.1f}", bpm, spo2);
    mqtt_manager_publish(TOPIC_VITALS, payload, 1, 0);
}

/* PUBLIC FUNCTION ------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "-------------------------------");
    ESP_LOGI(TAG, "        VITALS WATCH           ");
    ESP_LOGI(TAG, "-------------------------------");

    /* WiFi — tenta credenciais salvas; sobe AP se falhar ou não houver nenhuma */
    ESP_ERROR_CHECK(wifi_manager_init());
    wifi_manager_connect_from_nvs();

    /* MQTT e sensores — inicializados uma vez só se houver WiFi no boot */
    bool s_sensors_ready = false;

    if (wifi_manager_is_connected()) {
        const mqtt_config_t mqtt_cfg = {
            .uri         = BROKER ":" PORT,
            .client_id   = "vitals-watch-" PATIENT_ID,
            .username    = NULL,
            .password    = NULL,
            .ca_cert     = NULL,
            .client_cert = NULL,
            .client_key  = NULL,
        };
        /* Registra handlers antes do init — subscrições são feitas dentro do init */
        mqtt_manager_on_command           (on_cmd_command);
        mqtt_manager_on_config            (on_cmd_config);
        mqtt_manager_on_config_wifi       (on_cmd_config_wifi);
        mqtt_manager_on_config_sampling   (on_cmd_config_sampling);
        mqtt_manager_on_config_thresholds (on_cmd_config_thresholds);

        ESP_ERROR_CHECK(mqtt_manager_init(&mqtt_cfg));
        ESP_ERROR_CHECK(i2c_init());
        ESP_ERROR_CHECK(mpu6050_init(on_motion));
        ESP_ERROR_CHECK(max30102_init(on_vitals));
        s_sensors_ready = true;
        device_set_state(DEVICE_START);
    } else {
        ESP_LOGW(TAG, "Sem WiFi — STOP. Configure em http://192.168.4.1");
    }

    bool s_prev_wifi = wifi_manager_is_connected();

    while (1) {
        bool now_wifi = wifi_manager_is_connected();

        if (now_wifi && !s_prev_wifi) {
            /* WiFi reconectou */
            if (s_sensors_ready) {
                ESP_LOGI(TAG, "WiFi voltou — START");
                device_set_state(DEVICE_START);
            } else {
                /* Nunca teve WiFi no boot — precisa reiniciar para init completo */
                ESP_LOGI(TAG, "WiFi conectado pela primeira vez — reiniciando...");
                vTaskDelay(pdMS_TO_TICKS(500));
                device_set_state(DEVICE_REBOOT);
            }
        } else if (!now_wifi && s_prev_wifi) {
            /* WiFi caiu */
            ESP_LOGW(TAG, "WiFi perdido — STOP");
            device_set_state(DEVICE_STOP);
        }

        s_prev_wifi = now_wifi;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
