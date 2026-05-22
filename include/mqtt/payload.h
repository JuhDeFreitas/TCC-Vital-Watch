#ifndef PAYLOAD_H
#define PAYLOAD_H

#include "sensors/max30102.h"

/**
 * @brief Gera payload JSON com dados do MAX30102.
 *
 * @param metrics       Estrutura com heart rate e SpO2.
 * @param buffer        Buffer de saída.
 * @param buffer_size   Tamanho do buffer.
 *
 * @return true se sucesso, false caso contrário.
 */
bool build_max30102_payload(const max30102_data_t *metrics, char *buffer, size_t buffer_size);


/**
 * @brief Gera payload JSON de alerta.
 *
 * @param severity      Nível do alerta (ex: "critical").
 * @param buffer        Buffer de saída.
 * @param buffer_size   Tamanho do buffer.
 *
 * @return true se sucesso, false caso contrário.
 */
bool build_alert_payload(const char *severity, char *buffer, size_t buffer_size);


#endif

