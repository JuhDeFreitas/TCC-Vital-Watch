#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

#define MQTT_BROKER_URI   "mqtt://broker.emqx.io:1883"
#define MQTT_TOPIC_PUBLISH       "health/DEVICE_ID/signal/SENSOR_TYPE"
#define MQTT_TOPIC_SUBSCRIBE        "health/DEVICE_ID/config/CONFIG_TYPE"
#define MQTT_CLIENT_ID    "esp32_client"

#endif