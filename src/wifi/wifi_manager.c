#include "wifi_manager.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WIFI_MGR";

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_RECONNECT_BIT  BIT2
#define NVS_NAMESPACE       "wifi_cfg"

// ─── Estado ──────────────────────────────────────────────────────────────────

static EventGroupHandle_t s_events         = NULL;
static httpd_handle_t     s_httpd          = NULL;
static bool               s_connected      = false;
static bool               s_ap_active      = false;
static bool               s_auto_reconnect = false;  // só ativo após 1ª conexão ok

// ─── Página de configuração ───────────────────────────────────────────────────

static const char s_page_config[] =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Vitals Watch</title>"
    "<style>"
    "*{box-sizing:border-box}"
    "body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#f0f2f5;"
         "min-height:100vh;display:flex;align-items:center;justify-content:center;margin:0}"
    ".card{background:#fff;border-radius:14px;padding:36px 28px;width:100%;"
          "max-width:360px;box-shadow:0 4px 24px rgba(0,0,0,.10)}"
    ".logo{font-size:1.5em;font-weight:700;color:#4361ee;margin:0 0 4px}"
    "p{margin:0 0 28px;color:#888;font-size:.88em}"
    "label{display:block;font-size:.82em;font-weight:600;color:#555;margin-bottom:5px}"
    "input{width:100%;padding:11px 14px;border:1.5px solid #e0e0e0;border-radius:9px;"
          "font-size:.97em;margin-bottom:18px;outline:none;transition:border .2s}"
    "input:focus{border-color:#4361ee}"
    "button{width:100%;padding:13px;background:#4361ee;color:#fff;border:none;"
           "border-radius:9px;font-size:1em;font-weight:600;cursor:pointer;letter-spacing:.3px}"
    "button:active{background:#3451d1}"
    "</style></head><body>"
    "<div class='card'>"
    "<div class='logo'>Vitals Watch</div>"
    "<p>Configure a rede WiFi do dispositivo.</p>"
    "<form method='POST' action='/configure'>"
    "<label>Nome da rede (SSID)</label>"
    "<input type='text' name='ssid' placeholder='Minha Rede' required maxlength='32'>"
    "<label>Senha</label>"
    "<input type='password' name='password' placeholder='Senha da rede' maxlength='64'>"
    "<button type='submit'>Conectar</button>"
    "</form>"
    "</div></body></html>";

static const char s_page_success[] =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Configurado</title>"
    "<style>"
    "body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#f0f2f5;"
         "min-height:100vh;display:flex;align-items:center;justify-content:center;margin:0}"
    ".card{background:#fff;border-radius:14px;padding:40px 28px;width:100%;"
          "max-width:360px;box-shadow:0 4px 24px rgba(0,0,0,.10);text-align:center}"
    ".icon{font-size:3.2em;color:#4caf50;margin-bottom:12px}"
    "h2{color:#2e7d32;margin:0 0 10px;font-size:1.3em}"
    "p{color:#888;font-size:.88em;margin:0}"
    "</style></head><body>"
    "<div class='card'>"
    "<div class='icon'>&#10003;</div>"
    "<h2>Configuracao enviada!</h2>"
    "<p>O dispositivo vai conectar a rede.<br>Esta conexao sera encerrada em instantes.</p>"
    "</div></body></html>";

static const char s_page_error[] =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Erro</title>"
    "<style>"
    "body{font-family:-apple-system,sans-serif;background:#f0f2f5;"
         "min-height:100vh;display:flex;align-items:center;justify-content:center;margin:0}"
    ".card{background:#fff;border-radius:14px;padding:40px 28px;width:100%;"
          "max-width:360px;box-shadow:0 4px 24px rgba(0,0,0,.10);text-align:center}"
    ".icon{font-size:3em;color:#e53935;margin-bottom:12px}"
    "h2{color:#c62828;margin:0 0 10px}"
    "a{color:#4361ee;text-decoration:none;font-size:.9em}"
    "</style></head><body>"
    "<div class='card'>"
    "<div class='icon'>&#10007;</div>"
    "<h2>SSID invalido</h2>"
    "<a href='/'>Tentar novamente</a>"
    "</div></body></html>";

// ─── NVS ─────────────────────────────────────────────────────────────────────

static void nvs_save_credentials(const char *ssid, const char *password)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, "ssid",     ssid);
    nvs_set_str(h, "password", password);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Credenciais salvas no NVS");
}

static bool nvs_load_credentials(char *ssid, size_t ssid_len,
                                  char *password, size_t pass_len)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    bool ok = (nvs_get_str(h, "ssid",     ssid,     &ssid_len) == ESP_OK &&
               nvs_get_str(h, "password", password, &pass_len) == ESP_OK &&
               strlen(ssid) > 0);
    nvs_close(h);
    return ok;
}

// ─── URL decode e parse de form ───────────────────────────────────────────────

static void url_decode(char *dst, const char *src, size_t dst_len)
{
    size_t di = 0;
    for (size_t si = 0; src[si] && di < dst_len - 1; si++) {
        if (src[si] == '+') {
            dst[di++] = ' ';
        } else if (src[si] == '%' && src[si+1] && src[si+2]) {
            char hex[3] = { src[si+1], src[si+2], '\0' };
            dst[di++] = (char)strtol(hex, NULL, 16);
            si += 2;
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

static void extract_field(const char *body, const char *key,
                           char *out, size_t out_len)
{
    char search[48];
    snprintf(search, sizeof(search), "%s=", key);
    const char *start = strstr(body, search);
    if (!start) { out[0] = '\0'; return; }
    start += strlen(search);
    const char *end = strchr(start, '&');
    size_t len = end ? (size_t)(end - start) : strlen(start);
    if (len >= out_len) len = out_len - 1;
    char encoded[256] = {0};
    memcpy(encoded, start, len);
    url_decode(out, encoded, out_len);
}

// ─── Event handler ───────────────────────────────────────────────────────────

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Conectado — IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        s_connected      = true;
        s_auto_reconnect = true;   // a partir daqui, quedas ativam reconexão automática
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        xEventGroupSetBits(s_events, WIFI_FAIL_BIT);
        if (s_auto_reconnect && !s_ap_active) {
            xEventGroupSetBits(s_events, WIFI_RECONNECT_BIT);
        }
        ESP_LOGW(TAG, "Desconectado do AP");

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Cliente conectado ao AP");
    }
}

// ─── HTTP handlers ────────────────────────────────────────────────────────────

static esp_err_t handler_get_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, s_page_config, HTTPD_RESP_USE_STRLEN);
}

typedef struct { char ssid[33]; char password[65]; } reconnect_args_t;

// Executado 1.5 s após o POST para dar tempo da resposta HTTP ser enviada
static void reconnect_task(void *arg)
{
    reconnect_args_t *a = (reconnect_args_t *)arg;
    vTaskDelay(pdMS_TO_TICKS(1500));
    wifi_manager_stop_ap();
    wifi_manager_connect(a->ssid, a->password);
    free(a);
    vTaskDelete(NULL);
}

static esp_err_t handler_post_configure(httpd_req_t *req)
{
    char body[300] = {0};
    int  received  = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char ssid[33]     = {0};
    char password[65] = {0};
    extract_field(body, "ssid",     ssid,     sizeof(ssid));
    extract_field(body, "password", password, sizeof(password));

    if (strlen(ssid) == 0) {
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, s_page_error, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Credenciais recebidas via AP: ssid='%s'", ssid);
    nvs_save_credentials(ssid, password);

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, s_page_success, HTTPD_RESP_USE_STRLEN);

    reconnect_args_t *args = malloc(sizeof(reconnect_args_t));
    if (args) {
        strlcpy(args->ssid,     ssid,     sizeof(args->ssid));
        strlcpy(args->password, password, sizeof(args->password));
        xTaskCreate(reconnect_task, "wifi_reconnect", 4096, args, 5, NULL);
    }

    return ESP_OK;
}

static const httpd_uri_t s_uri_get = {
    .uri = "/", .method = HTTP_GET, .handler = handler_get_index
};
static const httpd_uri_t s_uri_post = {
    .uri = "/configure", .method = HTTP_POST, .handler = handler_post_configure
};

// ─── Servidor HTTP ────────────────────────────────────────────────────────────

static void httpd_server_start(void)
{
    if (s_httpd) return;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&s_httpd, &cfg) == ESP_OK) {
        httpd_register_uri_handler(s_httpd, &s_uri_get);
        httpd_register_uri_handler(s_httpd, &s_uri_post);
        ESP_LOGI(TAG, "HTTP server ativo — acesse http://192.168.4.1/");
    }
}

static void httpd_server_stop(void)
{
    if (!s_httpd) return;
    httpd_stop(s_httpd);
    s_httpd = NULL;
}

// ─── Task de reconexão automática ────────────────────────────────────────────
//
// Fica bloqueada aguardando WIFI_RECONNECT_BIT.
// O bit é setado pelo event handler apenas quando havia uma conexão ativa
// e ela cair (s_auto_reconnect=true). Tentativas durante o connect inicial
// não ativam esta task.
//
// Fluxo:
//   1. Chama esp_wifi_connect() (WiFi já está em modo STA e iniciado)
//   2. Aguarda WIFI_CONNECTED_BIT por WIFI_CONNECT_TIMEOUT_MS (20 s)
//   3. Se reconectou → volta a bloquear
//   4. Se timeout → para WiFi e sobe AP de configuração

static void auto_reconnect_task(void *arg)
{
    while (1) {
        xEventGroupWaitBits(s_events, WIFI_RECONNECT_BIT,
                            pdTRUE, pdFALSE, portMAX_DELAY);

        ESP_LOGI(TAG, "WiFi caiu — tentando reconectar (%d s)...",
                 WIFI_CONNECT_TIMEOUT_MS / 1000);

        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
        esp_wifi_connect();

        EventBits_t bits = xEventGroupWaitBits(
            s_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE,
            pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Reconectado com sucesso");
        } else {
            ESP_LOGW(TAG, "Reconexao falhou — subindo modo AP");
            s_auto_reconnect = false;
            esp_wifi_stop();
            wifi_manager_start_ap();
        }
    }
}

// ─── API pública ─────────────────────────────────────────────────────────────

esp_err_t wifi_manager_init(void)
{
    // NVS (necessário para WiFi e para salvar credenciais)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) return ret;

    ESP_ERROR_CHECK(esp_netif_init());

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    s_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    xTaskCreate(auto_reconnect_task, "wifi_reconnect", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "WiFi manager inicializado");
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    wifi_config_t sta = {0};
    strlcpy((char *)sta.sta.ssid,     ssid,     sizeof(sta.sta.ssid));
    strlcpy((char *)sta.sta.password, password, sizeof(sta.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Conectando a '%s' (timeout %d s)...",
             ssid, WIFI_CONNECT_TIMEOUT_MS / 1000);

    EventBits_t bits = xEventGroupWaitBits(
        s_events,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) return ESP_OK;

    ESP_LOGW(TAG, "Nao conectou em %d s — subindo modo AP",
             WIFI_CONNECT_TIMEOUT_MS / 1000);
    esp_wifi_stop();
    wifi_manager_start_ap();
    return ESP_ERR_TIMEOUT;
}

esp_err_t wifi_manager_connect_from_nvs(void)
{
    char ssid[33]     = {0};
    char password[65] = {0};

    if (!nvs_load_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
        ESP_LOGI(TAG, "Sem credenciais salvas — iniciando modo AP");
        wifi_manager_start_ap();
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Credenciais carregadas do NVS: ssid='%s'", ssid);
    return wifi_manager_connect(ssid, password);
}

void wifi_manager_start_ap(void)
{
    if (s_ap_active) return;

    s_auto_reconnect = false;
    esp_wifi_stop();

    wifi_config_t ap = {
        .ap = {
            .ssid           = WIFI_AP_SSID,
            .ssid_len       = strlen(WIFI_AP_SSID),
            .channel        = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode       = WIFI_AUTH_OPEN,
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_ap_active = true;
    ESP_LOGI(TAG, "AP '%s' ativo — conecte e acesse http://192.168.4.1/",
             WIFI_AP_SSID);

    httpd_server_start();
}

void wifi_manager_stop_ap(void)
{
    if (!s_ap_active) return;
    httpd_server_stop();
    esp_wifi_stop();
    s_ap_active = false;
    ESP_LOGI(TAG, "Modo AP encerrado");
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}
