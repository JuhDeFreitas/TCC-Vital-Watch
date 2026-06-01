#include <stdio.h>

#include "driver/i2c.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "wifi.h"
#include "mqtt/mqtt.h"
#include "device_info.h"
#include "i2c.h"

#include "sensors/mpu6050.h"
#include "sensors/motion_detector.h"
#include "sensors/max30102.h"
#include "sensors/max30102_driver.h"

#include "alert_manager.h"
#include "device_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"


/* =========================================================
 * I2C CONFIGURATION
 * ========================================================= */

#define PATIENT_ID "patient_001"


/* =========================================================
 * EXTERNAL TASK DECLARATIONS
 * ========================================================= */

/* MPU6050 motion processing task */
extern void mpu_motion_task(void *pvParameters);

/* MPU6050 task handle */
extern TaskHandle_t mpu_motion_task_handle;


/* =========================================================
 * GLOBAL VARIABLES
 * ========================================================= */

static const char *TAG = "MAIN";

/* Global I2C bus mutex */
SemaphoreHandle_t i2c_mutex = NULL;

/* Sensor task handles */
TaskHandle_t max30102_handle = NULL;


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

    /** =====================================================
     * I2C BUS INITIALIZATION
     * ===================================================== */

    i2c_start();

    /* =====================================================
     * MPU6050 - Motion Sensor Initialization
     * ===================================================== */

    /* Initialize MPU6050 driver */
    mpu_init();

    /* Configure interrupt GPIO and ISR handler */
    mpu_gpio_interrupt_init();

    /* Create motion processing task */
    xTaskCreate(
        mpu_motion_task,
        "MPU6050 Motion Task",
        4096,
        NULL,
        5,
        &mpu_motion_task_handle
    );

    /* Allow task and ISR stabilization */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Enable and configure motion interrupt detection */
    mpu_config_motion_interrupt();

    /* =====================================================
     * MAX30102 - Biometric Sensor Initialization
     * ===================================================== */

    /* Create MAX30102 acquisition task */
    xTaskCreate(
        max30102_task,
        "MAX30102 Task",
        8192,
        NULL,
        5,
        &max30102_handle
    );
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

    /* Initialize communication and sensor modules */
    wifi_wait_for_connection();

    /* Initialize FSM */
    device_state_manager_init();

    mqtt_init();
    sensors_init();

    /* Create alert manager task */
    xTaskCreate(
        alert_manager_task,
        "alert_manager_task",
        4096,
        NULL,
        5,
        NULL
    );

    ESP_LOGI(TAG, "System started");

    set_device_state(DEVICE_START);

    ESP_LOGI(TAG, "System ready. Waiting for MQTT commands...");

    /* Main loop */
    while (1)
    {
         ESP_LOGI(TAG, " ");
         vTaskDelay(pdMS_TO_TICKS(2000));
    }
}