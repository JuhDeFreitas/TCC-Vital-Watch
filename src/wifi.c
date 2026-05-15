#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "device_state.h"
#include "time_sync.h"

#include "wifi.h"
#include "mqtt/mqtt.h"

static const char *TAG = "WIFI";
static bool s_wifi_connected = false;

static bool dns_test(void)
{
    struct addrinfo hints = {0};
    struct addrinfo *res = NULL;

    int err = getaddrinfo("broker.emqx.io", NULL, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS falhou para broker.emqx.io, err=%d", err);
        return false;
    }

    ESP_LOGI(TAG, "DNS OK.");
    freeaddrinfo(res);
    return true;
}

static bool socket_test(void)
{
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;

    int err = getaddrinfo("broker.emqx.io", "1883", &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE(TAG, "Falha ao resolver broker para teste TCP, err=%d", err);
        return false;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Falha ao criar socket");
        freeaddrinfo(res);
        return false;
    }

    err = connect(sock, res->ai_addr, res->ai_addrlen);
    if (err == 0) {
        ESP_LOGI(TAG, "TCP OK: conexão com broker funcionando.");
        close(sock);
        freeaddrinfo(res);
        return true;
    } else {
        ESP_LOGE(TAG, "TCP falhou: sem acesso ao broker na porta 1883");
        close(sock);
        freeaddrinfo(res);
        return false;
    } 
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    { 
        ESP_LOGI(TAG, "Driver Wi-Fi iniciado");

        esp_err_t err = esp_wifi_connect();

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Tentando conectar ao AP...");
        }
        else {
            ESP_LOGE(TAG, "Falha ao iniciar conexão Wi-Fi: %s", esp_err_to_name(err));
        }
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;

        ESP_LOGW(TAG,"Wi-Fi desconectado. Motivo: %d", event->reason);

        s_wifi_connected = false;

        set_device_state(DEVICE_STOP);

        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_wifi_connect();
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

        ESP_LOGI(TAG, "Conectado. IP: " IPSTR, IP2STR(&event->ip_info.ip));

        if (dns_test() && socket_test()) {
            s_wifi_connected = true;
            time_sync_init();
        }
        else{
            s_wifi_connected = false;
        }
        
        //set_device_state(DEVICE_START);
    }
}

void wifi_init(void)
{   
    /* Desligando LOGS de inicialização do Driver Wi-fi */
    esp_log_level_set("wifi", ESP_LOG_NONE);  
    esp_log_level_set("wifi_init", ESP_LOG_NONE);
    esp_log_level_set("phy_init", ESP_LOG_NONE);
    esp_log_level_set("net80211", ESP_LOG_NONE);

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Apagando NVS...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_LOGI(TAG, "Inicializando driver Wi-Fi...");
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                    WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                    IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};

    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_LOGI(TAG, "Configurando SSID e senha...");
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
}

void wifi_start(void)
{
    //ESP_LOGI(TAG, "Iniciando Wi-Fi...");
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool wifi_is_connected(void)
{
    return s_wifi_connected;
}