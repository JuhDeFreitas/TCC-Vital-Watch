#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

/** Password and Wi-Fi network SSID */
#define WIFI_SSID     "Julia"
#define WIFI_PASSWORD "13020011"


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
void wifi_reconect(void);

#endif