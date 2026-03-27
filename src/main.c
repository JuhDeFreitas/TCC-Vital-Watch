#include <stdio.h>
#include "wifi.h"
#include "mqtt.h"


void app_main() {
  wifi_init();
  mqtt_init();

  for(int i = 0; i < 100; i++) {
    printf("\nOlá Mundo!\n");
    mqtt_publish_message("Olá Mundo!");
  }
}