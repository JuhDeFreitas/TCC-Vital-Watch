#include <stdio.h>

#include "driver/i2c.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "wifi.h"
#include "wifi_provisioning.h"
#include "mqtt/mqtt.h"
#include "device_info.h"
#include "i2c.h"

#include "sensors/mpu6050.h"
#include "sensors/motion_detector.h"
#include "sensors/max30102.h"
#include "sensors/max30102_driver.h"

#include "alert_manager.h"
#include "device_controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"


/* =========================================================
 * VARIABLE DECLARATIONS
 * ========================================================= */

static const char *TAG = "MAIN";

#define PATIENT_ID "patient_001"

/* MPU6050 motion processing task */
extern void mpu_motion_task(void *pvParameters);


/* =========================================================
 * GLOBAL VARIABLES
 * ========================================================= */


/* Global I2C bus mutex */
SemaphoreHandle_t i2c_mutex = NULL;



/* =========================================================
 * AUXILIARY FUNCTIONS
 * ========================================================= */

/**
 * @brief Initializes the non-volatile storage (NVS) for persistent data storage.
 */
 void NVM_storage_init(void){

    /* Initialize NVS flash storage.*/
    esp_err_t err = nvs_flash_init();

    if (err != ESP_OK){
        ESP_LOGW(TAG, "Failed to initialize NVS");
        return;
    }
  }

/**
 * @brief Waits until the Wi-Fi connection is established.
 */
void wifi_wait_for_connection(void)
{   
    wifi_init();

    while (!wifi_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));

    }

    ESP_LOGI(TAG, "Wi-Fi connected");
    ESP_LOGI(TAG, " ");
}


/**
 * @brief Initializes all system sensors and related tasks.
 */
void sensors_init(void)
{
    ESP_LOGI(TAG, "Initializing sensors...");

    i2c_start();
    mpu_task_init();
    max30102_task_init();
}


/* =========================================================
 * MAIN APPLICATION
 * ========================================================= */

void app_main(void)
{
    ESP_LOGI(TAG, " ");
    ESP_LOGI(TAG, " ");
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "=========== Vital Watch =========");
    ESP_LOGI(TAG, "=================================");    
    ESP_LOGI(TAG, " ");

    /* Initialize persistent storage */
    NVM_storage_init();

    /* Load configuration from NVS */
    load_device_id();
    load_sampling_config();
    load_wifi_config();
    load_threshold_config();

    /* Initialize AP mode */
    //wifi_provisioning_start();
    //vTaskDelay(pdMS_TO_TICKS(70000));

    /* Initialize communication and sensor modules */
    wifi_wait_for_connection();

    mqtt_init();
    
    sensors_init();

    alert_task_init();

    vTaskDelay(pdMS_TO_TICKS(2000));
    set_device_state(DEVICE_START);

    ESP_LOGI(TAG, "System started");

    ESP_LOGI(TAG, "System ready. Waiting for MQTT commands...");

    /* Main loop */
    while (1)
    {
         ESP_LOGI(TAG, " ");
         vTaskDelay(pdMS_TO_TICKS(2000));
    }
}