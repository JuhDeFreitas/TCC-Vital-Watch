
#include "device_info.h"
#include <stdio.h>
#include <string.h>

#include "esp_mac.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "mbedtls/sha256.h"

/**=========================================================
 * NVS Configuration
 * =========================================================*/

#define NVS_NAMESPACE "device"
#define NVS_KEY_ID    "device_id"

/* Secret String */
#define DEVICE_SECRET "VITALWATCH_SECRET_2026"

/**=========================================================
 * Global Variables
 * =========================================================*/

static const char *TAG = "DEVICE_INFO";

static char device_id[32];

/** ==============================================
 *  Auxiliary functions
 * ==============================================*/

 /**
  * * @brief Generates a hashed device ID from the ESP32 MAC.
  */
static void generate_hashed_id(void)
{
    uint8_t mac[6];

    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    char input[64];

    snprintf(input,
             sizeof(input),
             "%02X%02X%02X%02X%02X%02X%s",
             mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5],
             DEVICE_SECRET);

    uint8_t hash[32];

    mbedtls_sha256(
        (const unsigned char *)input,
        strlen(input),
        hash,
        0
    );

    snprintf(device_id,
             sizeof(device_id),
             "VW_%02X%02X%02X%02X",
             hash[0],
             hash[1],
             hash[2],
             hash[3]);
}


/**
 * @brief Initializes the device identity by loading it from NVS 
 * or generating a new one if not found.
 */
void device_id_init(void)
{

    nvs_handle_t nvs_handle;

    size_t required_size = sizeof(device_id);

    /* Open NVS namespace.*/
    esp_err_t err = nvs_open(
        NVS_NAMESPACE,
        NVS_READWRITE,
        &nvs_handle
    );

    if (err != ESP_OK){
        ESP_LOGE(TAG, "Failed to open NVS");
        return;
    }

     /* Try loading existing ID. */
    err = nvs_get_str(
        nvs_handle,
        NVS_KEY_ID,
        device_id,
        &required_size
    );

    /* Existing ID found. */
    if (err == ESP_OK){
        ESP_LOGI(TAG, " ID = %s", device_id);

        nvs_close(nvs_handle);

        return;
    }

    /* No ID found. Generate and store a new one. */
    generate_hashed_id();

    nvs_set_str(
        nvs_handle,
        NVS_KEY_ID,
        device_id
    );

    nvs_commit(nvs_handle);

    ESP_LOGI(TAG, "Generated new ID: %s", device_id);

    nvs_close(nvs_handle);
}


/**
 * @brief Returns the device ID.
 */
const char *device_get_id(void)
{
    return device_id;
}