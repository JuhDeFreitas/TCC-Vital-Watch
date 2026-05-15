#include "mqtt/mqtt.h"

#include "mqtt_client.h"
#include "esp_log.h"
#include "device_state.h"

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = NULL;
static bool mqtt_connected = false;

static void mqtt_event_handler(void *handler_args,  esp_event_base_t base,  int32_t event_id,  void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_DATA: {
            ESP_LOGI(TAG, "Mensagem recebida!");
            
            ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);

            ESP_LOGI(TAG,"mqtt event data ativado...");

            char data[event->data_len + 1];

            memcpy(data, event->data, event->data_len);
            data[event->data_len] = '\0';

            ESP_LOGI(TAG, "CMD: %s", data);

            if (strcmp(data, "START") == 0) {
                set_device_state(DEVICE_START);

            } else if (strcmp(data, "STOP") == 0) {
                set_device_state(DEVICE_STOP);

            } else if (strcmp(data, "REBOOT") == 0) {
                set_device_state(DEVICE_REBOOT);
            }
            break;
        }

        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            ESP_LOGI(TAG, "Conectado ao broker MQTT");
            mqtt_subscribe(TOPIC_COMMAND);
            mqtt_subscribe(TOPIC_CONFIG);
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
    ESP_LOGI(TAG, "Broker URI: %s", BROKER);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER,
        .credentials.client_id = PATIENT_ID
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
    mqtt_start();
}

void mqtt_publish_message(const char *topic, const void *payload)
{
    int len = strlen((const char *)payload);
    if (client == NULL) {
        ESP_LOGW(TAG, "Cliente MQTT nulo");
        return;
    }

    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT ainda não conectado, publish ignorado");
        return;
    }

    int msg_id = esp_mqtt_client_publish(client, topic, (const char *)payload, len, 1, 0);

    if (msg_id < 0) {
        ESP_LOGE(TAG, "Falha ao publicar mensagem");
    } else {
        //ESP_LOGI(TAG, "Mensagem enfileirada, msg_id=%d, len=%d", msg_id, (int)len);
    }
}

void mqtt_subscribe(char *topic)
{
    if (client == NULL) {
        ESP_LOGW(TAG, "Cliente MQTT nulo");
        return;
    }

    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT ainda não conectado, subscribe ignorado");
        return;
    }

    int msg_id = esp_mqtt_client_subscribe(client, topic, 1);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Falha ao se inscrever no tópico");
    } else {
        ESP_LOGI(TAG, "Inscrição solicitada no tópico: %s", topic);
    }
}

void mqtt_stop(void)
{
    if (client != NULL) {
        esp_mqtt_client_stop(client);
        //esp_mqtt_client_destroy(client);
        //client = NULL;
        mqtt_connected = false;
        ESP_LOGI(TAG, "Cliente MQTT parado.");
    }
}

void mqtt_start(void)
{
    if (client != NULL) {
        esp_err_t err = esp_mqtt_client_start(client);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Erro ao iniciar MQTT: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Cliente MQTT iniciado.");
        }
    } else {
        ESP_LOGW(TAG, "Cliente MQTT nulo, não é possível iniciar.");
    }
}