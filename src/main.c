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

#define WIFI_BOOT_TIMEOUT_MS 20000

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

void NVM_storage_init(void){

    esp_err_t err = nvs_flash_init();

    if (err != ESP_OK){
        ESP_LOGW(TAG, "Failed to initialize NVS");
        return;
    }
  }

void sensors_init(void)
{
    ESP_LOGI(TAG, "Initializing sensors...");

    i2c_start();
    mpu_task_init();
    max30102_task_init();
}

/* =========================================================
 * WIFI WITH AP FALLBACK
 * ========================================================= */

/**
 * @brief Poll wifi_is_connected() for up to timeout_ms milliseconds.
 * @return true if connected within the timeout, false otherwise.
 */
static bool wifi_wait_timeout(uint32_t timeout_ms)
{
    uint32_t start = xTaskGetTickCount();

    while (!wifi_is_connected()) {
        uint32_t elapsed = (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
        if (elapsed >= timeout_ms) return false;
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    return true;
}

/**
 * @brief Connect to WiFi. If the connection fails within 20 s, enter AP
 *        provisioning mode, serve the configuration page and wait for valid
 *        credentials. Blocks until a working network connection is established.
 */
static void wifi_connect_or_provision(void)
{
    wifi_init();

    ESP_LOGI(TAG, "Waiting up to %d s for WiFi...", WIFI_BOOT_TIMEOUT_MS / 1000);

    if (wifi_wait_timeout(WIFI_BOOT_TIMEOUT_MS)) {
        ESP_LOGI(TAG, "WiFi connected with saved credentials");
        return;
    }

    /* Could not connect — start AP provisioning mode */
    ESP_LOGW(TAG, "No connection after %d s. Entering AP mode.",
             WIFI_BOOT_TIMEOUT_MS / 1000);

    wifi_provisioning_start();

    while (!wifi_is_connected()) {
        char new_ssid[32] = {0};
        char new_pass[64] = {0};

        /* Block until the user submits the web form */
        wifi_provisioning_wait_credentials(new_ssid, sizeof(new_ssid),
                                           new_pass, sizeof(new_pass));

        ESP_LOGI(TAG, "Received SSID: %s — testing connection...", new_ssid);

        wifi_reconfigure(new_ssid, new_pass);

        if (wifi_wait_timeout(WIFI_BOOT_TIMEOUT_MS)) {
            /* Connection successful: save credentials and leave AP mode */
            wifi_save_current_config();
            ESP_LOGI(TAG, "Connected! Stopping AP mode.");
            wifi_provisioning_stop();

            /* Wait for the STA to reconnect after AP mode teardown */
            while (!wifi_is_connected()) {
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        } else {
            ESP_LOGW(TAG, "Connection failed. Staying in AP mode.");
        }
    }
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

    /* Connect to WiFi or launch AP provisioning if no network is reachable */
    wifi_connect_or_provision();

    ESP_LOGI(TAG, "Wi-Fi connected");
    ESP_LOGI(TAG, " ");

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
