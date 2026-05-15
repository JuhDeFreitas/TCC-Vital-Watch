#ifndef PAYLOAD_H
#define PAYLOAD_H

#include "sensors/max30102.h"

/**
 * @brief Monta o payload de dados dos dados vitais para envio MQTT.
 * 
 * @return Retorna o payload no formato:
 *   {
 *   "heart_rate": 75,
 *   "spo2": 98,
 *   "timestamp": 1710000000
 *   }
 */
bool build_max30102_payload(const max30102_data_t *metrics, char *buffer, size_t buffer_size);

#endif

