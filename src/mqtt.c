#include "mqtt/mqtt.h"
#include "mqtt/mqtt_config.h"

#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = NULL;
static bool mqtt_connected = false;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            ESP_LOGI(TAG, "Conectado ao broker MQTT");
            esp_mqtt_client_subscribe(client, MQTT_TOPIC_SUBSCRIBE, 1);
            break;

        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            ESP_LOGW(TAG, "Desconectado do broker MQTT");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Subscribe realizado, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Publish realizado, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Erro no MQTT");
            break;

        default:
            break;
    }
}

void mqtt_init(void)
{
    ESP_LOGI(TAG, "Inicializando MQTT...");
    ESP_LOGI(TAG, "Broker URI: %s", MQTT_BROKER_URI);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = MQTT_CLIENT_ID
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Erro ao criar cliente MQTT");
        return;
    }

    esp_err_t err = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao registrar eventos MQTT: %s", esp_err_to_name(err));
        return;
    }

    err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao iniciar MQTT: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Cliente MQTT iniciado");
}

void mqtt_publish_message(const char *payload)
{
    if (client == NULL) {
        ESP_LOGW(TAG, "Cliente MQTT nulo");
        return;
    }

    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT ainda não conectado, publish ignorado");
        return;
    }

    int msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC_PUBLISH, payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Falha ao publicar mensagem");
    } else {
        ESP_LOGI(TAG, "Mensagem enfileirada, msg_id=%d, payload=%s", msg_id, payload);
    }
}

void mqtt_subscribe(void)
{
    if (client == NULL) {
        ESP_LOGW(TAG, "Cliente MQTT nulo");
        return;
    }

    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT ainda não conectado, subscribe ignorado");
        return;
    }

    int msg_id = esp_mqtt_client_subscribe(client, MQTT_TOPIC_SUBSCRIBE, 1);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Falha ao se inscrever no tópico");
    } else {
        ESP_LOGI(TAG, "Inscrição solicitada no tópico: %s", MQTT_TOPIC_SUBSCRIBE);
    }
}