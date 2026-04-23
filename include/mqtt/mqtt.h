#ifndef MQTT_H
#define MQTT_H

/* Função para construir o tópico MQTT */
void build_mqtt_topic(char *topic, size_t size,
                      const char *device_id,
                      const char *category);
void mqtt_init(void);
void mqtt_publish_message(const char* payload);
void mqtt_subscribe();

#endif


