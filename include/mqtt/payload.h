#ifndef PAYLOAD_H
#define PAYLOAD_H

#include "sensors/max30102.h"

/**
 * @brief Generates JSON payload with MAX30102 sensor data.
 *
 * @param metrics       Structure containing heart rate and SpO2.
 * @param buffer        Output buffer.
 * @param buffer_size   Size of the output buffer.
 *
 * @return true if successful, false otherwise.
 */
bool build_max30102_payload(const max30102_data_t *metrics, char *buffer, size_t buffer_size);


/**
 * @brief Generates JSON payload for alerts.
 *
 * @param type          Alert type (e.g., "heart_rate", "spo2").
 * @param severity      Alert severity level (e.g., "critical").
 * @param buffer        Output buffer.
 * @param buffer_size   Size of the output buffer.
 *
 * @return true if successful, false otherwise.
 */
bool build_alert_payload(
                        const char *type, 
                        const char *severity, 
                        char *buffer, 
                        size_t buffer_size);


/**
 * @brief Parses sampling configuration JSON payload.
 */
bool parse_sampling_config(const char *data);

/**
 * @brief Parses Wi-Fi configuration JSON payload.
 */
bool parse_wifi_config(const char *data);

/**
 * @brief Parses threshold configuration JSON payload.
 */
bool parse_threshold_config(const char *data);

#endif