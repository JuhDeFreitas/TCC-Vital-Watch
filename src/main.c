#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "i2c.h"
#include "max30102_api.h"
#include "mpu6050.h"

static const char *TAG = "MAIN";

static void on_step_detected(uint32_t cadence_spm)
{
    if (cadence_spm >= 120)
        ESP_LOGI("MPU6050", "Corrida! cadencia=%lu spm", (unsigned long)cadence_spm);
    else if (cadence_spm >= 80)
        ESP_LOGI("MPU6050", "Caminhada rapida: %lu spm", (unsigned long)cadence_spm);
    else
        ESP_LOGI("MPU6050", "Passo detectado");
}

static void on_vitals(int bpm, double spo2)
{
    if (bpm < 0 || spo2 < 0.0) {
        ESP_LOGW(TAG, "Sinal fraco ou dedo ausente (bpm=%d, spo2=%.1f)", bpm, spo2);
        return;
    }
    
    ESP_LOGI(TAG, "BPM: %d | SpO2: %.1f%%", bpm, spo2);
}

void app_main(void)
{
    ESP_LOGI(TAG, "-------------------------------");
    ESP_LOGI(TAG, "        VITALS WATCH           ");
    ESP_LOGI(TAG, "-------------------------------");

    ESP_ERROR_CHECK(i2c_init());
    ESP_ERROR_CHECK(mpu6050_init(on_step_detected));
    ESP_ERROR_CHECK(max30102_init(on_vitals));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
