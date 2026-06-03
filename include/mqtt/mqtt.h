#ifndef MQTT_H
#define MQTT_H

#include <sys/types.h>

/** \brief ID do dispositivo */
#define PATIENT_ID "patient_001"

/* Configurações do MQTT */
#define BROKER "mqtt://broker.hivemq.com"
#define PORT 1883

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

/* Funções do MQTT */

void build_mqtt_topic(char *topic, size_t size,
                      const char *device_id,
                      const char *category);
void mqtt_init(void);
void mqtt_publish_message(const char *topic,
                          const void *payload);
void mqtt_subscribe(const char *topic);
void mqtt_stop(void);
void mqtt_disconnect(void);
void mqtt_reconnect(void);
void mqtt_start(void);

#endif
