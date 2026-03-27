#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

#define MQTT_BROKER_URI   "mqtt://localhost:1883"
#define MQTT_TOPIC_PUBLISH       "tcc/{$DEVICE_ID}/{$SENSOR_TYPE}"
#define MQTT_TOPIC_SUBSCRIBE        "tcc/config/{$DEVICE_ID}/set"
#define MQTT_CLIENT_ID    "esp32_client"

#endif