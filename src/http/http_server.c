#include <string.h>
#include <stdio.h>

#include "esp_http_server.h"
#include "esp_log.h"

#include "wifi_provisioning.h"

static const char *TAG = "HTTP";
static httpd_handle_t s_server = NULL;

/* =========================================================
 * HTML PAGES
 * ========================================================= */

static const char *HTML_ROOT =
    "<!DOCTYPE html><html lang='pt-BR'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>VitalWatch Setup</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;display:flex;justify-content:center;"
    "align-items:center;min-height:100vh;margin:0;background:#f0f0f0}"
    ".box{background:#fff;padding:2rem;border-radius:8px;"
    "box-shadow:0 2px 8px rgba(0,0,0,.2);width:300px;box-sizing:border-box}"
    "h2{margin:0 0 1.5rem;text-align:center;color:#333}"
    "label{display:block;margin:.5rem 0 .2rem;color:#555;font-size:.9rem}"
    "input{width:100%;padding:.5rem;border:1px solid #ccc;border-radius:4px;"
    "box-sizing:border-box;font-size:1rem}"
    "button{width:100%;padding:.75rem;background:#0066cc;color:#fff;border:none;"
    "border-radius:4px;font-size:1rem;cursor:pointer;margin-top:1rem}"
    "button:active{background:#004a99}"
    "</style></head><body><div class='box'>"
    "<h2>VitalWatch Setup</h2>"
    "<form method='POST' action='/connect'>"
    "<label>Rede WiFi (SSID)</label>"
    "<input name='ssid' placeholder='Nome da rede' required autocomplete='off'>"
    "<label>Senha</label>"
    "<input name='pass' type='password' placeholder='Senha da rede' autocomplete='off'>"
    "<button type='submit'>Conectar</button>"
    "</form></div></body></html>";

static const char *HTML_CONNECTING =
    "<!DOCTYPE html><html lang='pt-BR'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta http-equiv='refresh' content='22;url=/'>"
    "<title>VitalWatch</title>"
    "<style>body{font-family:Arial,sans-serif;text-align:center;padding:2rem;"
    "background:#f0f0f0}.box{background:#fff;padding:2rem;border-radius:8px;"
    "display:inline-block;box-shadow:0 2px 8px rgba(0,0,0,.2)}"
    "h2{color:#0066cc}p{color:#555}</style>"
    "</head><body><div class='box'>"
    "<h2>Conectando...</h2>"
    "<p>Aguarde enquanto o dispositivo testa as credenciais.</p>"
    "<p>Se este ponto de acesso desaparecer, a conexao foi bem-sucedida!</p>"
    "<p><small>Se permanecer, as credenciais estao incorretas."
    " Redirecionando em 22s...</small></p>"
    "</div></body></html>";

/* =========================================================
 * URL DECODE
 * ========================================================= */

static void url_decode(const char *src, char *dst, size_t max_len)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < max_len - 1; i++) {
        if (src[i] == '%' && src[i + 1] && src[i + 2]) {
            unsigned char h = (unsigned char)src[++i];
            unsigned char l = (unsigned char)src[++i];
            h = (h >= 'A') ? ((h & 0xDFu) - 'A' + 10) : (h - '0');
            l = (l >= 'A') ? ((l & 0xDFu) - 'A' + 10) : (l - '0');
            dst[j++] = (char)((h << 4) | l);
        } else if (src[i] == '+') {
            dst[j++] = ' ';
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

static void parse_field(const char *body, const char *key,
                        char *value, size_t max_len)
{
    char prefix[40];
    snprintf(prefix, sizeof(prefix), "%s=", key);

    const char *start = strstr(body, prefix);
    if (!start) return;

    start += strlen(prefix);
    const char *end = strchr(start, '&');
    size_t raw_len  = end ? (size_t)(end - start) : strlen(start);

    char raw[128] = {0};
    if (raw_len >= sizeof(raw)) raw_len = sizeof(raw) - 1;
    strncpy(raw, start, raw_len);

    url_decode(raw, value, max_len);
}

/* =========================================================
 * HANDLERS
 * ========================================================= */

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_ROOT, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t connect_handler(httpd_req_t *req)
{
    char body[256] = {0};
    int  len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) return ESP_FAIL;
    body[len] = '\0';

    char ssid[32] = {0};
    char pass[64] = {0};

    parse_field(body, "ssid", ssid, sizeof(ssid));
    parse_field(body, "pass", pass, sizeof(pass));

    ESP_LOGI(TAG, "Credentials received  SSID: %s", ssid);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_CONNECTING, HTTPD_RESP_USE_STRLEN);

    wifi_provisioning_notify_credentials(ssid, pass);

    return ESP_OK;
}

/* =========================================================
 * SERVER LIFECYCLE
 * ========================================================= */

void http_server_start(void)
{
    if (s_server) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t root = {
        .uri     = "/",
        .method  = HTTP_GET,
        .handler = root_handler,
    };

    httpd_uri_t connect = {
        .uri     = "/connect",
        .method  = HTTP_POST,
        .handler = connect_handler,
    };

    httpd_register_uri_handler(s_server, &root);
    httpd_register_uri_handler(s_server, &connect);

    ESP_LOGI(TAG, "HTTP server ready at http://192.168.4.1");
}

void http_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}
