#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

// Número máximo de tópicos subscritos simultaneamente
#define MQTT_MAX_SUBSCRIPTIONS  8

/* ID do dispositivo */
#define PATIENT_ID "patient_001"

/* Configurações do MQTT */
#define BROKER "mqtt://broker.hivemq.com"
#define PORT "1883"   // string — permite BROKER ":" PORT na URI

/* Monta base */
#define BASE_TOPIC "VitalsWatch/" PATIENT_ID

/* Publish */
#define TOPIC_VITALS  BASE_TOPIC "/vitals"
#define TOPIC_ALERTS  BASE_TOPIC "/alerts"

/* Subscribe */
#define TOPIC_COMMAND BASE_TOPIC "/command"
#define TOPIC_CONFIG  BASE_TOPIC "/config"

/* Config Especific topics */
#define TOPIC_CONFIG_WIFI         TOPIC_CONFIG "/wifi"
#define TOPIC_CONFIG_SAMPLING     TOPIC_CONFIG "/sampling"
#define TOPIC_CONFIG_THRESHOLDS   TOPIC_CONFIG "/thresholds"

// Callback invocado quando uma mensagem chega num tópico subscrito.
// topic e payload NÃO são null-terminated — use topic_len e payload_len.
// Suporta wildcards MQTT: + (um nível) e # (todos os níveis restantes).
typedef void (*mqtt_message_handler_t)(const char *topic,   int topic_len,
                                        const char *payload, int payload_len);

// Handler simplificado para tópicos de controle — recebe apenas o payload.
typedef void (*mqtt_topic_handler_t)(const char *payload, int payload_len);

// ─── Handlers dos tópicos de controle padrão ────────────────────────────────
// Registre antes ou depois de mqtt_manager_init().
// O sistema subscreve automaticamente esses tópicos ao iniciar.
void mqtt_manager_on_command           (mqtt_topic_handler_t handler);
void mqtt_manager_on_config            (mqtt_topic_handler_t handler);
void mqtt_manager_on_config_wifi       (mqtt_topic_handler_t handler);
void mqtt_manager_on_config_sampling   (mqtt_topic_handler_t handler);
void mqtt_manager_on_config_thresholds (mqtt_topic_handler_t handler);

typedef struct {
    const char *uri;          // "mqtts://broker.exemplo.com:8883"
    const char *client_id;
    const char *username;     // NULL → sem autenticação por usuário
    const char *password;     // NULL → sem autenticação por senha
    const char *ca_cert;      // PEM do CA do broker  (obrigatório para TLS)
    const char *client_cert;  // PEM do cert do cliente (NULL → sem mTLS)
    const char *client_key;   // PEM da chave privada   (NULL → sem mTLS)
} mqtt_config_t;

// ─── API ────────────────────────────────────────────────────────────────────

// Inicializa o cliente MQTT com TLS e conecta ao broker.
// Reconexão automática está habilitada por padrão.
esp_err_t mqtt_manager_init(const mqtt_config_t *config);

// Publica payload num tópico.
// qos: 0, 1 ou 2  |  retain: 0 ou 1
// Retorna ESP_ERR_INVALID_STATE se não estiver conectado.
esp_err_t mqtt_manager_publish(const char *topic, const char *payload,
                                int qos, int retain);

// Subscreve um tópico e registra o handler para mensagens recebidas.
// Suporta wildcards MQTT (+ e #).
// Máximo MQTT_MAX_SUBSCRIPTIONS subscrições simultâneas.
esp_err_t mqtt_manager_subscribe(const char *topic,
                                  mqtt_message_handler_t handler, int qos);

// Remove subscrição de um tópico.
void mqtt_manager_unsubscribe(const char *topic);

bool mqtt_manager_is_connected(void);

// Para e libera o cliente MQTT.
void mqtt_manager_deinit(void);

#endif
