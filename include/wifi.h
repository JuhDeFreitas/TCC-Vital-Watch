#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

#define DEFAULT_WIFI_SSID      "Julia"
#define DEFAULT_WIFI_PASSWORD  "13020011"

/** Password and Wi-Fi network SSID */
typedef struct {

    char ssid[32];
    char password[64];

} vitalwatch_wifi_config_t;

extern vitalwatch_wifi_config_t g_wifi_config;

/* ===================================================
 * PUBLIC FUNCTIONS 
 * =================================================== */

 /**
  * @brief Update Wi-Fi configuration in to global config structure.
  * @param data JSON string containing new Wi-Fi config (SSID and password).
  */
bool set_wifi_config(const char *data);

/**
 * @brief Load Wi-Fi configuration from NVS and apply it to the global config structure.
 */
bool load_wifi_config(void);

/**
 * @brief Initialize the Wi-Fi subsystem and configure the station mode.
 */
void wifi_init(void);

/**
 * @brief Start the Wi-Fi driver.
 */
void wifi_start(void);

/**
 * @brief Check whether the device is connected to a Wi-Fi network.
 *
 * @return true if connected.
 * @return false otherwise.
 */
bool wifi_is_connected(void);

/**
 * @brief Attempt to reconnect to the configured Wi-Fi network.
 */
bool wifi_reconnect(void);

#endif