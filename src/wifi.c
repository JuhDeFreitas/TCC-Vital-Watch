#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "device_controller.h"
#include "time_sync.h"
#include "wifi.h"
#include "mqtt/mqtt.h"
#include "mqtt/payload.h"

static const char *TAG = "WIFI";
static bool s_wifi_connected = false;

static uint32_t s_connect_start_time = 0;

vitalwatch_wifi_config_t g_wifi_config = {
    .ssid = "Julia",
    .password = "13020011"
};

 /* ========================================================= 
  * WIFI CONFIG 
  * ========================================================= */

#define NVS_NAMESPACE "wifi_config"
#define NVS_KEY_WIFI_CONFIG "config"


static bool save_wifi_config(const vitalwatch_wifi_config_t *config)
{
    nvs_handle_t nvs_handle;

    esp_err_t err = nvs_open(
        NVS_NAMESPACE,
        NVS_READWRITE,
        &nvs_handle);

    if (err != ESP_OK) {
        return false;
    }

    err = nvs_set_blob(
        nvs_handle,
        NVS_KEY_WIFI_CONFIG,
        config,
        sizeof(*config));

    if (err == ESP_OK) {
        nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);

    return err == ESP_OK;
}

static void set_wifi_to_default(void)
{
    //strncpy(g_wifi_config.ssid, DEFAULT_WIFI_SSID, sizeof(g_wifi_config.ssid) - 1);
    //strncpy(g_wifi_config.password, DEFAULT_WIFI_PASSWORD, sizeof(g_wifi_config.password) - 1);

    g_wifi_config = (vitalwatch_wifi_config_t){
            .ssid = DEFAULT_WIFI_SSID,
            .password = DEFAULT_WIFI_PASSWORD
        };

    ESP_LOGI(TAG, "SSID: %s", g_wifi_config.ssid);
    
    save_wifi_config(&g_wifi_config);
}

bool set_wifi_config(const char *data)
{   
    /* Backup current configuration */
    //vitalwatch_wifi_config_t previous_config = g_wifi_config;

    /* Parse received JSON into g_wifi_config (configuration structure) */
    if (!parse_wifi_config(data)) {
        ESP_LOGW(TAG, "Failed to update Wi-Fi configuration.");
        return false;
    }

    /* Try to conect to new network */
    wifi_connect();
    vTaskDelay(pdMS_TO_TICKS(200));

    while( wifi_verify_timeout() == false) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    if (wifi_is_connected()) {
        save_wifi_config(&g_wifi_config);

        ESP_LOGI(TAG, "Wi-Fi config saved");

        return true;
    }

    set_wifi_to_default();
    esp_restart();

    return false;
}

bool load_wifi_config(void)
{
    nvs_handle_t nvs_handle;

    size_t required_size = sizeof(g_wifi_config);

    esp_err_t err = nvs_open(
        NVS_NAMESPACE,
        NVS_READWRITE,
        &nvs_handle
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS");
        return false;
    }

    err = nvs_get_blob(
        nvs_handle,
        NVS_KEY_WIFI_CONFIG,
        &g_wifi_config,
        &required_size
    );

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi config loaded");
        ESP_LOGI(TAG, "SSID: %s", g_wifi_config.ssid);

        nvs_close(nvs_handle);
        return true;
    }

    ESP_LOGW(TAG, "Wi-Fi config not found. Using defaults config.");



    nvs_close(nvs_handle);
    return false;
}


/* =========================================================
 * NETWORK VALIDATION
 * ========================================================= */

static bool dns_test(void)
{
    struct addrinfo hints = {0};
    struct addrinfo *res = NULL;

    int err = getaddrinfo("broker.emqx.io", NULL, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS resolution failed for broker.emqx.io, err=%d", err);
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
        ESP_LOGE(TAG, "Failed to resolve broker for TCP test, err=%d", err);
        return false;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        freeaddrinfo(res);
        return false;
    }

    err = connect(sock, res->ai_addr, res->ai_addrlen);
    if (err == 0) {
        ESP_LOGI(TAG, "TCP OK: broker connection successful.");
        close(sock);
        freeaddrinfo(res);
        return true;
    } else {
        ESP_LOGE(TAG, "TCP failed: no access to broker on port 1883");
        close(sock);
        freeaddrinfo(res);
        return false;
    }
}

/* =========================================================
 * WIFI EVENT HANDLER
 * ========================================================= */

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Wi-Fi driver started.");
        s_wifi_connected = false;

        esp_wifi_connect();

    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        s_wifi_connected = false;

        set_device_state(DEVICE_STOP);

        wifi_connect();

        if(wifi_verify_timeout() == true && wifi_is_connected() == false) {
            set_wifi_to_default();
            esp_restart();
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "MASK: " IPSTR, IP2STR(&event->ip_info.netmask));

        s_connect_start_time = 0;

        if (dns_test() && socket_test()) {
            s_wifi_connected = true;
            time_sync_init();
        } else {
            s_wifi_connected = false;
        }
    }
}

/* =========================================================
 * WIFI CORE
 * ========================================================= */

void wifi_init(void)
{
    /* Disable verbose Wi-Fi logs */
    esp_log_level_set("wifi", ESP_LOG_NONE);
    esp_log_level_set("wifi_init", ESP_LOG_NONE);
    esp_log_level_set("phy_init", ESP_LOG_NONE);
    esp_log_level_set("net80211", ESP_LOG_NONE);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_NONE);

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Erasing NVS...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_LOGI(TAG, " ");
    ESP_LOGI(TAG, "Initializing Wi-Fi driver.");
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL
    ));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        NULL
    ));

    wifi_config_t wifi_config = {0};

    strncpy((char *)wifi_config.sta.ssid,
            g_wifi_config.ssid,
            sizeof(wifi_config.sta.ssid) - 1);

    strncpy((char *)wifi_config.sta.password,
            g_wifi_config.password,
            sizeof(wifi_config.sta.password) - 1);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_LOGI(TAG, "Configuring SSID and password.");
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    wifi_start();
}

/* =========================================================
 * WIFI CONTROL
 * ========================================================= */

void wifi_start(void)
{
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool wifi_connect(void)
{
    if(s_connect_start_time == 0) {
        s_connect_start_time = xTaskGetTickCount();
    }

    ESP_LOGI(TAG, "Attempting Wi-Fi connection...");

    esp_err_t err = esp_wifi_connect();

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to start Wi-Fi connection: %s",
                 esp_err_to_name(err));
        return false;
    }

    return true;
}

void wifi_disconnect(void)
{
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    s_wifi_connected = false;
}

bool wifi_verify_timeout(void)
{
    if (s_connect_start_time == 0) return true;

    uint32_t elapsed_ms = (xTaskGetTickCount() - s_connect_start_time) * portTICK_PERIOD_MS;

    if (elapsed_ms >= 30000)
    {
        s_connect_start_time = 0;
        return true;
    }

    return false;
}


bool wifi_is_connected(void)
{
    return s_wifi_connected;
}

