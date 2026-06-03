#include <string.h>

#include "wifi_provisioning.h"
#include "http/http_server.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"

#define WIFI_SSID "ESP_AP"
#define WIFI_PASS "12345678"

static const char *TAG = "wifi_prov";

void wifi_provisioning_start(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = 1,
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "AP iniciado");

    // 👇 chama o servidor separado
    http_server_start();
}