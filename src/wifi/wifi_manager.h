#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

// Nome do AP de configuração (sem senha — aberto para facilitar acesso)
#define WIFI_AP_SSID              "VitalsWatch"
#define WIFI_AP_CHANNEL           1
#define WIFI_AP_MAX_CONN          4
#define WIFI_CONNECT_TIMEOUT_MS   20000   // 20 s antes de subir o AP

// ─── API ────────────────────────────────────────────────────────────────────

// Inicializa NVS, TCP/IP stack, event loop e driver WiFi.
// Deve ser chamado uma vez antes de qualquer outra função deste módulo.
esp_err_t wifi_manager_init(void);

// Tenta conectar com as credenciais fornecidas.
// Se não conectar em WIFI_CONNECT_TIMEOUT_MS, sobe o modo AP automaticamente.
// Retorna ESP_OK se conectou, ESP_ERR_TIMEOUT se caiu no AP.
esp_err_t wifi_manager_connect(const char *ssid, const char *password);

// Carrega credenciais salvas no NVS e chama wifi_manager_connect().
// Se não houver credenciais salvas, sobe o AP diretamente.
esp_err_t wifi_manager_connect_from_nvs(void);

// Sobe o ponto de acesso "VitalsWatch" e o servidor HTTP de configuração.
// Acesse http://192.168.4.1/ para configurar a rede.
void wifi_manager_start_ap(void);

// Encerra o servidor HTTP e desliga o AP.
void wifi_manager_stop_ap(void);

bool wifi_manager_is_connected(void);

#endif
