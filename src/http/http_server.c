#include <string.h>
#include <stdio.h>

#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

/* HTML EMBUTIDO (simples por enquanto) */
static const char *html_page =
"<!DOCTYPE html><html><body>"
"<h2>Configurar WiFi</h2>"
"<form method='POST' action='/connect'>"
"SSID:<br><input name='ssid'><br>"
"Senha:<br><input name='pass' type='password'><br><br>"
"<input type='submit' value='Conectar'>"
"</form>"
"</body></html>";

/* GET / */
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* parsing simples */
static void parse_post(char *data, char *ssid, char *pass)
{
    sscanf(data, "ssid=%[^&]&pass=%s", ssid, pass);
}

/* POST /connect */
static esp_err_t connect_handler(httpd_req_t *req)
{
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (len <= 0) return ESP_FAIL;

    buf[len] = '\0';

    char ssid[32] = {0};
    char pass[64] = {0};

    parse_post(buf, ssid, pass);

    ESP_LOGI(TAG, "SSID: %s", ssid);
    ESP_LOGI(TAG, "PASS: %s", pass);

    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

void http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler
    };

    httpd_uri_t connect = {
        .uri = "/connect",
        .method = HTTP_POST,
        .handler = connect_handler
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &connect);

    ESP_LOGI(TAG, "HTTP Server iniciado");
}