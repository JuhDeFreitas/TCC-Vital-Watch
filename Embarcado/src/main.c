#include "wifi.h"
#include "mqtt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    wifi_init();
    wifi_start();

    mqtt_init();

    while (1) {
        mqtt_publish_message("Hello ESP32");
        vTaskDelay(pdMS_TO_TICKS(5000)); // espera 5s
    }
}