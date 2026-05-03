#include <stdio.h>

#include "wifi.h"
#include "mqtt.h"
#include "i2c.h"
#include "mpu6050.h"
#include "max30102/max30102.h"
#include "max30102/max30102_driver.h"
#include "data_processing.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "device_state.h"

#include "esp_log.h"

static const char *TAG = "MAIN";

#define PATIENT_ID "patient_001"

// Mutex global do barramento I2C
SemaphoreHandle_t i2c_mutex = NULL;

TaskHandle_t mpu6050_handle = NULL;
TaskHandle_t max30102_handle = NULL;

/* Funções auxiliares */

/*
void set_device_state(device_state_t new_state)
{
    device_state = new_state;
}*/

void start_sensors(void)
{
    ESP_LOGI(TAG, "Iniciando sensores...");
    xTaskCreate(
        mpu6050_task,
        "MPU6050 Task",
        4096,
        NULL,
        5,
        &mpu6050_handle   
    );

    xTaskCreate(
        max30102_task,
        "MAX30102 Task",
        8192,
        NULL,
        5,
        &max30102_handle  
    );
}


void device_task(void *pvParameters)
{
    /* Thread para atualizar/acompanhar o estado do dispositivo */
    while (1)
    {
        switch (device_state)
        {
            case DEVICE_STOP:
                // não faz nada
                if (mpu6050_handle != NULL) {
                    vTaskSuspend(mpu6050_handle);
                }
                if (max30102_handle != NULL) {
                    vTaskSuspend(max30102_handle);
                }
                // Add: aciona função de lowPower
                // Acompanha somente o config via mqtt
                vTaskDelay(pdMS_TO_TICKS(500));
                break;

            case DEVICE_START:
                ESP_LOGI(TAG, "Rodando coleta...");

                if (mpu6050_handle != NULL) {
                    vTaskResume(mpu6050_handle);
                }
                if (max30102_handle != NULL) {
                    vTaskResume(max30102_handle);
                }
                //process_data();

                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case DEVICE_REBOOT:
                ESP_LOGW(TAG, "Reiniciando...");
                vTaskDelay(pdMS_TO_TICKS(100)); // pequeno delay
                esp_restart();
                break;

            default:
                break;
        }
    }
}



/* Funções principais */
void app_main(void)
{
    ESP_LOGI(TAG, "_________ Vital Watch _________");

    /* Logica de inicialização do device */
    wifi_init();
    wifi_start();
    device_state = DEVICE_START;

    if(wifi_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi conectado. ");
        if (device_state == DEVICE_START) {
            /* Starta a aplicação */
            ESP_LOGI(TAG, "Iniciando aplicação...");
            mqtt_init();
            start_i2c();

            start_sensors();

            xTaskCreate(device_task, "device_task", 4096, NULL, 5, NULL);
        }
    } else {
        /* Entra em AP MODE para configuração de rede Wi-fi*/
        /*...*/
        ESP_LOGW(TAG, "Wi-Fi não conectado. Apliacação não iniciada.");
    }


}

