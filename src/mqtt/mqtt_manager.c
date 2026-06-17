#include "mqtt_manager.h"

#include "mqtt_client.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MQTT_MGR";

// ─── Estado interno ──────────────────────────────────────────────────────────

typedef struct {
    char                    topic[128];
    mqtt_message_handler_t  handler;
    int                     qos;
    bool                    active;
} mqtt_sub_t;

static esp_mqtt_client_handle_t s_client    = NULL;
static bool                     s_connected = false;
static mqtt_sub_t               s_subs[MQTT_MAX_SUBSCRIPTIONS];

// ─── Wildcard matching ───────────────────────────────────────────────────────
//
// Implementa a especificação MQTT de wildcards:
//   +  → casa exatamente um nível de tópico (ex: "sensors/+/temp")
//   #  → casa tudo a partir desse ponto, deve ser o último caracter
//
// Exemplos:
//   "sensor/+/temp"  casa "sensor/sala/temp"
//   "sensor/#"       casa "sensor/sala/temp/raw"
//   "#"              casa qualquer tópico

static bool topic_matches(const char *pattern, const char *topic)
{
    if (*pattern == '#') return true;

    if (*pattern == '+') {
        // Avança o tópico até a próxima '/' ou fim
        while (*topic && *topic != '/') topic++;
        pattern++;
        // Após '+' esperamos '/' ou fim em ambos
        if (*pattern == '\0' && *topic == '\0') return true;
        if (*pattern == '/' && *topic == '/') {
            return topic_matches(pattern + 1, topic + 1);
        }
        return false;
    }

    if (*pattern == '\0' && *topic == '\0') return true;
    if (*pattern == '\0' || *topic == '\0') return false;
    if (*pattern != *topic) return false;

    return topic_matches(pattern + 1, topic + 1);
}

// ─── Re-subscribe após reconexão ────────────────────────────────────────────

static void resubscribe_all(void)
{
    for (int i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (s_subs[i].active) {
            esp_mqtt_client_subscribe(s_client, s_subs[i].topic, s_subs[i].qos);
            ESP_LOGD(TAG, "Re-subscrito: %s", s_subs[i].topic);
        }
    }
}

// ─── Event handler ───────────────────────────────────────────────────────────

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "Conectado ao broker");
        resubscribe_all();
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "Desconectado — aguardando reconexao automatica...");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "Subscribe confirmado (msg_id=%d)", ev->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "Publish confirmado (msg_id=%d)", ev->msg_id);
        break;

    case MQTT_EVENT_DATA: {
        // ev->topic e ev->data NÃO são null-terminated — copiamos para comparar
        char topic_buf[128] = {0};
        int  tlen = ev->topic_len < (int)(sizeof(topic_buf) - 1)
                        ? ev->topic_len : (int)(sizeof(topic_buf) - 1);
        memcpy(topic_buf, ev->topic, tlen);

        bool dispatched = false;
        for (int i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
            if (s_subs[i].active && topic_matches(s_subs[i].topic, topic_buf)) {
                s_subs[i].handler(ev->topic,  ev->topic_len,
                                   ev->data,   ev->data_len);
                dispatched = true;
            }
        }
        if (!dispatched) {
            ESP_LOGW(TAG, "Mensagem em topico sem handler: %.*s", tlen, topic_buf);
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        if (ev->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Erro TLS — esp_tls=%d  errno=%d",
                     ev->error_handle->esp_tls_last_esp_err,
                     ev->error_handle->esp_transport_sock_errno);
        } else if (ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGE(TAG, "Conexao recusada (codigo=%d)",
                     ev->error_handle->connect_return_code);
        }
        break;

    default:
        break;
    }
}

// ─── Handlers dos tópicos de controle padrão ────────────────────────────────

static mqtt_topic_handler_t s_on_command           = NULL;
static mqtt_topic_handler_t s_on_config            = NULL;
static mqtt_topic_handler_t s_on_config_wifi       = NULL;
static mqtt_topic_handler_t s_on_config_sampling   = NULL;
static mqtt_topic_handler_t s_on_config_thresholds = NULL;


static void wrap_command(const char *t, int tl, const char *p, int pl)
    { (void)t; (void)tl; if (s_on_command)           s_on_command(p, pl);           else ESP_LOGW(TAG, "Sem handler: " TOPIC_COMMAND);           }

static void wrap_config(const char *t, int tl, const char *p, int pl)
    { (void)t; (void)tl; if (s_on_config)            s_on_config(p, pl);            else ESP_LOGW(TAG, "Sem handler: " TOPIC_CONFIG);            }

static void wrap_config_wifi(const char *t, int tl, const char *p, int pl)
    { (void)t; (void)tl; if (s_on_config_wifi)       s_on_config_wifi(p, pl);       else ESP_LOGW(TAG, "Sem handler: " TOPIC_CONFIG_WIFI);       }

static void wrap_config_sampling(const char *t, int tl, const char *p, int pl)
    { (void)t; (void)tl; if (s_on_config_sampling)   s_on_config_sampling(p, pl);   else ESP_LOGW(TAG, "Sem handler: " TOPIC_CONFIG_SAMPLING);   }

static void wrap_config_thresholds(const char *t, int tl, const char *p, int pl)
    { (void)t; (void)tl; if (s_on_config_thresholds) s_on_config_thresholds(p, pl); else ESP_LOGW(TAG, "Sem handler: " TOPIC_CONFIG_THRESHOLDS); }

void mqtt_manager_on_command           (mqtt_topic_handler_t h) { s_on_command           = h; }
void mqtt_manager_on_config            (mqtt_topic_handler_t h) { s_on_config            = h; }
void mqtt_manager_on_config_wifi       (mqtt_topic_handler_t h) { s_on_config_wifi       = h; }
void mqtt_manager_on_config_sampling   (mqtt_topic_handler_t h) { s_on_config_sampling   = h; }
void mqtt_manager_on_config_thresholds (mqtt_topic_handler_t h) { s_on_config_thresholds = h; }

// ─── API pública ─────────────────────────────────────────────────────────────

esp_err_t mqtt_manager_init(const mqtt_config_t *config)
{
    if (!config || !config->uri) return ESP_ERR_INVALID_ARG;

    memset(s_subs, 0, sizeof(s_subs));

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri            = config->uri,
            .verification.certificate = config->ca_cert,
        },
        .credentials = {
            .client_id              = config->client_id,
            .username               = config->username,
            .authentication = {
                .password           = config->password,
                .certificate        = config->client_cert,
                .key                = config->client_key,
            }
        },
        .session.keepalive              = 30,
        .network.reconnect_timeout_ms   = 5000,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_client) return ESP_FAIL;

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_client));

    // Subscreve cada tópico de controle com seu wrapper dedicado.
    // Ficam registrados em s_subs e são re-enviados ao broker após reconexão.
    mqtt_manager_subscribe(TOPIC_COMMAND,           wrap_command,           1);
    mqtt_manager_subscribe(TOPIC_CONFIG,            wrap_config,            1);
    mqtt_manager_subscribe(TOPIC_CONFIG_WIFI,       wrap_config_wifi,       1);
    mqtt_manager_subscribe(TOPIC_CONFIG_SAMPLING,   wrap_config_sampling,   1);
    mqtt_manager_subscribe(TOPIC_CONFIG_THRESHOLDS, wrap_config_thresholds, 1);

    ESP_LOGI(TAG, "MQTT iniciado → %s", config->uri);
    return ESP_OK;
}

esp_err_t mqtt_manager_publish(const char *topic, const char *payload,
                                int qos, int retain)
{
    if (!s_connected) {
        ESP_LOGW(TAG, "Publish ignorado — nao conectado (%s)", topic);
        return ESP_ERR_INVALID_STATE;
    }

    int msg_id = esp_mqtt_client_publish(s_client, topic, payload,
                                          (int)strlen(payload), qos, retain);
    if (msg_id < 0) return ESP_FAIL;

    ESP_LOGD(TAG, "Publicado [qos=%d]: %s = %s", qos, topic, payload);
    return ESP_OK;
}

esp_err_t mqtt_manager_subscribe(const char *topic,
                                  mqtt_message_handler_t handler, int qos)
{
    if (!topic || !handler) return ESP_ERR_INVALID_ARG;

    // Atualiza se já existir
    for (int i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (s_subs[i].active && strcmp(s_subs[i].topic, topic) == 0) {
            s_subs[i].handler = handler;
            s_subs[i].qos     = qos;
            return ESP_OK;
        }
    }

    // Novo slot
    for (int i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (!s_subs[i].active) {
            strlcpy(s_subs[i].topic, topic, sizeof(s_subs[i].topic));
            s_subs[i].handler = handler;
            s_subs[i].qos     = qos;
            s_subs[i].active  = true;
            if (s_connected) {
                esp_mqtt_client_subscribe(s_client, topic, qos);
            }
            ESP_LOGI(TAG, "Subscrito: %s (qos=%d)", topic, qos);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "Limite de subscricoes atingido (%d)", MQTT_MAX_SUBSCRIPTIONS);
    return ESP_ERR_NO_MEM;
}

void mqtt_manager_unsubscribe(const char *topic)
{
    for (int i = 0; i < MQTT_MAX_SUBSCRIPTIONS; i++) {
        if (s_subs[i].active && strcmp(s_subs[i].topic, topic) == 0) {
            s_subs[i].active = false;
            if (s_connected) {
                esp_mqtt_client_unsubscribe(s_client, topic);
            }
            ESP_LOGI(TAG, "Desinscrito: %s", topic);
            return;
        }
    }
}

bool mqtt_manager_is_connected(void)
{
    return s_connected;
}

void mqtt_manager_deinit(void)
{
    if (!s_client) return;
    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client    = NULL;
    s_connected = false;
}
