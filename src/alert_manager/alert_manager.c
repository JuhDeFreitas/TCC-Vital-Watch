#include "alert_manager.h"
#include "config_manager.h"
#include "mqtt_manager.h"
#include "time_sync.h"

#include "esp_log.h"
#include <stdio.h>
#include <time.h>

static const char *TAG = "ALERT";

/* Leituras consecutivas fora do normal necessárias para disparar o alerta */
#define ALERT_DEBOUNCE_COUNT  3

/* Faixas de referência — OMS/AHA (valores reais vêm de g_alert_config):
 *
 * FC em repouso (BPM):  < 50 muito baixa | 50-59 baixa | 60-100 normal | 101-120 alta | >120 muito alta
 * FC em corrida (BPM):  <100 muito baixa | 100-109 baixa | 110-149 normal | 150-170 alta | >170 muito alta
 * SpO2 (%):             < 90 muito baixa | 90-94 baixa | >=95 normal
 */

/* Estado atual do paciente — atualizado pelo MPU6050 via alert_manager_set_state() */
volatile patient_state_t g_patient_state = PATIENT_RESTING;

typedef struct {
    int counter;
} alert_state_t;

static alert_state_t hr_alert   = {0};
static alert_state_t spo2_alert = {0};

/* -------------------------------------------------------------------
 * Publica o alerta no broker MQTT
 * ------------------------------------------------------------------- */
static void publish_alert(const char *type, const char *severity)
{
    char payload[128];
    time_t now = 0;
    time(&now);

    if (time_is_synced()) {
        snprintf(payload, sizeof(payload),
                 "{\"type\":\"%s\",\"severity\":\"%s\",\"timestamp\":%lld}",
                 type, severity, (long long)now);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"type\":\"%s\",\"severity\":\"%s\"}",
                 type, severity);
    }

    mqtt_manager_publish(TOPIC_ALERTS, payload, 1, 0);
    ESP_LOGW(TAG, "ALERTA [%s] severidade: %s", type, severity);
}

/* -------------------------------------------------------------------
 * Verifica frequência cardíaca levando em conta o estado de atividade
 * ------------------------------------------------------------------- */
static void check_hr(int bpm)
{
    const char *severity = NULL;

    if (g_patient_state == PATIENT_RUNNING) {
        /* FC durante corrida — limiares OMS/AHA para atividade física */
        if (bpm < g_alert_config.hr_running_very_low) {
            severity = "very_low";
        } else if (bpm < g_alert_config.hr_running_low) {
            severity = "low";
        } else if (bpm >= g_alert_config.hr_running_very_high) {
            severity = "very_high";
        } else if (bpm >= g_alert_config.hr_running_high) {
            severity = "high";
        }
    } else {
        /* FC em repouso — limiares OMS/AHA padrão */
        if (bpm < g_alert_config.hr_very_low) {
            severity = "very_low";
        } else if (bpm < g_alert_config.hr_low) {
            severity = "low";
        } else if (bpm >= g_alert_config.hr_very_high) {
            severity = "very_high";
        } else if (bpm >= g_alert_config.hr_high) {
            severity = "high";
        }
    }

    if (severity != NULL) {
        hr_alert.counter++;
        /* Confirma com 3 leituras consecutivas, depois dispara em toda leitura anormal */
        if (hr_alert.counter >= ALERT_DEBOUNCE_COUNT) {
            publish_alert("heart_rate", severity);
        }
    } else {
        hr_alert.counter = 0;
    }
}

/* -------------------------------------------------------------------
 * Verifica SpO2
 * ------------------------------------------------------------------- */
static void check_spo2(double spo2)
{
    const char *severity = NULL;

    if (spo2 <= g_alert_config.spo2_very_low) {
        severity = "very_low";
    } else if (spo2 <= g_alert_config.spo2_low) {
        severity = "low";
    }

    if (severity != NULL) {
        spo2_alert.counter++;
        if (spo2_alert.counter >= ALERT_DEBOUNCE_COUNT) {
            publish_alert("spo2", severity);
        }
    } else {
        spo2_alert.counter = 0;
    }
}

/* -------------------------------------------------------------------
 * API pública
 * ------------------------------------------------------------------- */
void alert_manager_set_state(patient_state_t state)
{
    g_patient_state = state;
    ESP_LOGI(TAG, "Estado: %s", state == PATIENT_RUNNING ? "CORRENDO" : "REPOUSO");
}

void alert_manager_check(int bpm, double spo2)
{
    if (bpm > 0)    check_hr(bpm);
    if (spo2 > 0.0) check_spo2(spo2);
}
