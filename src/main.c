#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "i2c.h"
#include "max30102_api.h"
#include "mpu6050.h"
#include "wifi_manager.h"

static const char *TAG = "MAIN";

/* PRIVATE FUNCTIONS ------------------------------------------------------------- */

static void on_motion(mpu6050_event_t event, uint32_t cadence_spm)
{
    if (event == MPU_EVENT_RUNNING)
        ESP_LOGI(TAG, " ");
        ESP_LOGI("MPU6050", "Correndo: %lu spm", (unsigned long)cadence_spm);
    else
        ESP_LOGI(TAG, " ");
        ESP_LOGI("MPU6050", "Parou de correr");w
}

static void on_vitals(int bpm, double spo2)
{
    if (bpm < 0 || spo2 < 0.0) {
        ESP_LOGW(TAG, " ");
        ESP_LOGW(TAG, "Sinal fraco ou dedo ausente (bpm=%d, spo2=%.1f)", bpm, spo2);
        return;
    }

    /* Call alert Manager*/

    /* MQTT Publish */
    ESP_LOGI(TAG, " ");
    ESP_LOGI(TAG, "BPM: %d | SpO2: %.1f%%", bpm, spo2);
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

    /* Inicilização dos sensores */
    ESP_ERROR_CHECK(i2c_init());
    ESP_ERROR_CHECK(mpu6050_init(on_motion));
    ESP_ERROR_CHECK(max30102_init(on_vitals));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
