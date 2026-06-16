#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include <stddef.h>

/**
 * @brief Start AP provisioning mode.
 * Assumes wifi_init() was already called. Switches the driver to APSTA,
 * starts the HTTP configuration page and waits for new credentials.
 */
void wifi_provisioning_start(void);

/**
 * @brief Stop AP provisioning mode and resume STA-only operation.
 */
void wifi_provisioning_stop(void);

/**
 * @brief Block until new credentials arrive from the HTTP form.
 */
void wifi_provisioning_wait_credentials(char *ssid, size_t ssid_len,
                                        char *pass, size_t pass_len);

/**
 * @brief Called by the HTTP server to deliver received credentials.
 */
void wifi_provisioning_notify_credentials(const char *ssid, const char *pass);

#endif
