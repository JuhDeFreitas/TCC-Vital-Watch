#include <string.h>

#include "wifi_provisioning.h"
#include "http/http_server.h"
#include "wifi.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define AP_SSID "VitalWatch_Setup"
#define AP_PASS "12345678"
#define AP_CHANNEL 6
#define CRED_RECEIVED_BIT BIT0

static const char *TAG = "WIFI_AP";

static EventGroupHandle_t s_cred_event = NULL;
static char s_ssid[32] = {0};
static char s_pass[64] = {0};
static esp_netif_t *s_ap_netif = NULL;

/* =========================================================
 * CREDENTIAL SIGNALING
 * ========================================================= */

void wifi_provisioning_notify_credentials(const char *ssid, const char *pass)
{
    if (!s_cred_event) return;

    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = '\0';
    strncpy(s_pass, pass, sizeof(s_pass) - 1);
    s_pass[sizeof(s_pass) - 1] = '\0';

    xEventGroupSetBits(s_cred_event, CRED_RECEIVED_BIT);
}

void wifi_provisioning_wait_credentials(char *ssid, size_t ssid_len,
                                        char *pass, size_t pass_len)
{
    xEventGroupWaitBits(s_cred_event, CRED_RECEIVED_BIT,
                        pdTRUE, pdFALSE, portMAX_DELAY);

    strncpy(ssid, s_ssid, ssid_len - 1);
    ssid[ssid_len - 1] = '\0';
    strncpy(pass, s_pass, pass_len - 1);
    pass[pass_len - 1] = '\0';
}

/* =========================================================
 * AP LIFECYCLE
 * ========================================================= */

void wifi_provisioning_start(void)
{
    if (!s_cred_event) {
        s_cred_event = xEventGroupCreate();
    }

    /* Prevent the STA disconnect handler from restarting the device */
    wifi_set_provisioning_mode(true);

    /* Add AP netif on top of the existing STA netif */
    s_ap_netif = esp_netif_create_default_wifi_ap();

    /* Restart driver in APSTA mode */
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_APSTA);

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid      = AP_SSID,
            .ssid_len  = strlen(AP_SSID),
            .channel   = AP_CHANNEL,
            .password  = AP_PASS,
            .max_connection = 4,
            .authmode  = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();

    ESP_LOGI(TAG, "AP started  SSID: %s  Pass: %s", AP_SSID, AP_PASS);
    ESP_LOGI(TAG, "Open http://192.168.4.1 to configure WiFi");

    http_server_start();
}

void wifi_provisioning_stop(void)
{
    http_server_stop();

    /* Keep provisioning_mode ON during stop/start so that the DISCONNECTED
     * event triggered by esp_wifi_stop() does not cause an unwanted restart. */
    esp_wifi_stop();

    if (s_ap_netif) {
        esp_netif_destroy(s_ap_netif);
        s_ap_netif = NULL;
    }

    esp_wifi_set_mode(WIFI_MODE_STA);

    /* Re-apply STA config with the freshly validated credentials */
    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid, g_wifi_config.ssid,
            sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, g_wifi_config.password,
            sizeof(sta_cfg.sta.password) - 1);
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta_cfg.sta.pmf_cfg.capable    = true;
    sta_cfg.sta.pmf_cfg.required   = false;
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);

    /* Clear provisioning flag before start so WIFI_EVENT_STA_START
     * triggers a normal connection attempt in the event handler. */
    wifi_set_provisioning_mode(false);

    esp_wifi_start();

    if (s_cred_event) {
        vEventGroupDelete(s_cred_event);
        s_cred_event = NULL;
    }

    ESP_LOGI(TAG, "AP stopped. Resuming STA mode.");
}
