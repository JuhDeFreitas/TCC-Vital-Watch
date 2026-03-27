#include <string.h>
#include "wifi.h"
#include "wifi_config.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

void wifi_init(void)
{
    esp_netif_init();                     // inicia rede
    esp_event_loop_create_default();      // loop de eventos
    esp_netif_create_default_wifi_sta(); // modo station

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);                  // inicializa wifi

    wifi_config_t wifi_config = {
        .sta = {}
    };

    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASSWORD);

    esp_wifi_set_mode(WIFI_MODE_STA);     // modo cliente
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
}

void wifi_start(void)
{
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}