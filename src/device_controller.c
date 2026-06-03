#include "device_controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "mqtt/mqtt.h"
#include "sensors/max30102.h"
#include "sensors/mpu6050.h"
#include "alert_manager.h"

static const char *TAG = "DEVICE_CONTROLLER";

/* Current device state */
static device_state_t device_state = DEVICE_STOP;


/* =========================================================
 * PRIVATE FUNCTIONS
 * ========================================================= */

static void handle_device_state(device_state_t new_state)
{
    switch (new_state)
    {
        case DEVICE_STOP:
            ESP_LOGI(TAG, "Stopping device...");

            mpu_motion_task_suspend();
            max30102_task_suspend();
            alert_task_suspend();

            break;

        case DEVICE_START:
            ESP_LOGI(TAG, "Starting device...");

            // mqtt_start();
            mpu_motion_task_resume();
            max30102_task_resume();
            alert_task_resume();

            break;

        case DEVICE_REBOOT:
            ESP_LOGW(TAG, "Rebooting...");

            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();

            break;

        default:
            ESP_LOGW(TAG, "Invalid device state");

            break;
    }
}


/* =========================================================
 * PUBLIC FUNCTIONS
 * ========================================================= */

void set_device_state(device_state_t new_state)
{
    if (device_state != new_state){

       device_state = new_state;

       handle_device_state(new_state); 
    }
}


device_state_t get_device_state(void)
{
    return device_state;
}