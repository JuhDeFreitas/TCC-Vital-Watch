#include "mqtt.h"
#include "mqtt_config.h"

#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = NULL;

void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);

    ESP_LOGI(TAG, "MQTT iniciado");
}

void mqtt_publish_message(const char* payload)
{
    if (client != NULL) {
        esp_mqtt_client_publish(client, MQTT_TOPIC_PUBLISH, payload, 0, 1, 0);
        ESP_LOGI(TAG, "Mensagem publicada: %s", payload);
    }
}

void mqtt_subscribe()
{
    if (client != NULL) {
        esp_mqtt_client_subscribe(client, MQTT_TOPIC_SUBSCRIBE, 1);
        ESP_LOGI(TAG, "Inscrito no tópico: %s", MQTT_TOPIC_SUBSCRIBE);
    }
}