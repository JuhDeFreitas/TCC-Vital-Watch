#include "device_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "DEVICE_STATE";

static volatile device_state_t device_state = DEVICE_STOP;

static TaskHandle_t device_state_task_handle = NULL;

extern TaskHandle_t mpu6050_handle;
extern TaskHandle_t max30102_handle;

extern void mqtt_start(void);
extern void mqtt_stop(void);

static void device_state_task(void *pvParameters)
{
    device_state_t last_state = DEVICE_STOP;

    while (1)
    {
        if (device_state != last_state)
        {
            switch (device_state)
            {
                case DEVICE_STOP:
                    ESP_LOGI(TAG, "Parando dispositivo...");

                    if (mpu6050_handle)
                        vTaskSuspend(mpu6050_handle);
                    if (max30102_handle)
                        vTaskSuspend(max30102_handle);

                    break;

                case DEVICE_START:
                    ESP_LOGI(TAG, "Iniciando dispositivo...");
                    //mqtt_start();

                    if (mpu6050_handle)
                        vTaskResume(mpu6050_handle);
                    if (max30102_handle)
                        vTaskResume(max30102_handle);

                    break;

                case DEVICE_REBOOT:
                    ESP_LOGW(TAG, "Reiniciando ESP...");

                    vTaskDelay(pdMS_TO_TICKS(400));
                    esp_restart();

                    break;

                default:

                    ESP_LOGW(TAG, "Estado inválido");

                    break;
            }

            last_state = device_state;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void device_state_init(void)
{
    xTaskCreate(
        device_state_task,
        "device_state_task",
        4096,
        NULL,
        5,
        &device_state_task_handle
    );

    ESP_LOGI(TAG, "Task de controle de estado criada");
}

void set_device_state(device_state_t new_state)
{
    device_state = new_state;

    //ESP_LOGI(TAG, "Novo estado: %d", new_state);
}

device_state_t get_device_state(void)
{
    return device_state;
}