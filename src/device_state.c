#include "device_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "DEVICE_STATE";

/* Current device state */
static volatile device_state_t device_state = DEVICE_STOP;

/* State manager task handle */
static TaskHandle_t device_state_task_handle = NULL;

/* External task handles */
extern TaskHandle_t mpu_motion_task_handle;
extern TaskHandle_t max30102_handle;

/* External MQTT control functions */
extern void mqtt_start(void);
extern void mqtt_stop(void);

/* =========================================================
 * DEVICE STATE TASK
 * ========================================================= */

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
                    ESP_LOGI(TAG, "Stopping device...");

                    if (mpu_motion_task_handle){
                        vTaskSuspend(mpu_motion_task_handle);
                    }

                    if (max30102_handle){
                        vTaskSuspend(max30102_handle);
                    }

                    break;

                case DEVICE_START:
                    ESP_LOGI(TAG, "Starting device...");

                    // mqtt_start();

                    if (mpu_motion_task_handle){
                        vTaskResume(mpu_motion_task_handle);
                    }

                    if (max30102_handle){
                        vTaskResume(max30102_handle);
                    }

                    break;

                case DEVICE_REBOOT:
                    ESP_LOGW(TAG, "Rebooting ESP32...");

                    vTaskDelay(pdMS_TO_TICKS(400));
                    esp_restart();

                    break;

                default:
                    ESP_LOGW(TAG, "Invalid device state");

                    break;
            }

            last_state = device_state;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* =========================================================
 * PUBLIC FUNCTIONS
 * ========================================================= */

/**
 * @brief Create the device state manager task.
 */
void device_state_manager_init(void)
{
    xTaskCreate(
        device_state_task,
        "device_state_task",
        4096,
        NULL,
        5,
        &device_state_task_handle
    );

    ESP_LOGI(TAG, "Device state task created");
}

/**
 * @brief Update the current device state.
 *
 * @param new_state New state to be applied.
 */
void set_device_state(device_state_t new_state)
{
    device_state = new_state;

    // ESP_LOGI(TAG, "New state: %d", new_state);
}

/**
 * @brief Get the current device state.
 *
 * @return Current device state.
 */
device_state_t get_device_state(void)
{
    return device_state;
}