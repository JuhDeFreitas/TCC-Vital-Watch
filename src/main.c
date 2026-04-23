#include <stdio.h>

#include "wifi/wifi.h"
#include "mqtt/mqtt.h"

#include "i2c/i2c.h"
#include "mpu6050/mpu6050.h"
#include "max3010x/max30102.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

static const char *TAG = "MAIN";

// Mutex global do barramento I2C
SemaphoreHandle_t i2c_mutex = NULL;

// Task do MAX30102
/*void max30102_task(void *pvParameters)
{
    uint32_t red = 0;
    uint32_t ir = 0;

    while (1) {
        if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
            if (max30102_read_sample(&red, &ir) == ESP_OK) {
                ESP_LOGI("MAX30102", "RED: %lu | IR: %lu",
                         (unsigned long)red,
                         (unsigned long)ir);
            } else {
                ESP_LOGE("MAX30102", "Erro ao ler amostra");
            }

            xSemaphoreGive(i2c_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}*/

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando aplicação...");
    wifi_init();
    wifi_start();
    mqtt_init();
    start_i2c();

    /*
    i2c_mutex = xSemaphoreCreateMutex();
    if (i2c_mutex == NULL) {
        ESP_LOGE(TAG, "Falha ao criar mutex do I2C");
        return;
    }
    
    // Inicializa MAX30102 protegido pelo mutex
    if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
        if (max30102_init() != ESP_OK) {
            ESP_LOGE(TAG, "Falha ao inicializar o MAX30102");
            xSemaphoreGive(i2c_mutex);
            return;
        }
        xSemaphoreGive(i2c_mutex);
    }

    
    xTaskCreate(
        start_mpu6050,
        "MPU6050 Task",
        4096,
        NULL,
        5,
        NULL
    );

    xTaskCreate(
        max30102_task,
        "MAX30102 Task",
        8192,
        NULL,
        5,
        NULL
    );
    */

    while (1) {
        mqtt_publish_message("Hello ESP32");
        ESP_LOGI(TAG, "Message published");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}