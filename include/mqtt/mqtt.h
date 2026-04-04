#ifndef MQTT_H
#define MQTT_H

void mqtt_init(void);
void mqtt_publish_message(const char* payload);
void mqtt_subscribe();

#endif


