#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// ─── Configuração de amostragem ───────────────────────────────────────────────
// Payload MQTT esperado (TOPIC_CONFIG_SAMPLING):
// {"sampling_interval_ms": 1000, "publish_interval_ms": 2000}
typedef struct {
    uint32_t sampling_interval_ms;
    uint32_t publish_interval_ms;
} sampling_config_t;

// ─── Configuração de limiares de alerta ──────────────────────────────────────
// Payload MQTT esperado (TOPIC_CONFIG_THRESHOLDS) — todos os campos são opcionais:
// {
//   "hr_very_low": 40, "hr_low": 50, "hr_normal": 100,
//   "hr_high": 130,    "hr_very_high": 160,
//   "spo2_very_low": 90, "spo2_low": 94, "spo2_normal": 100,
//   "motion_threshold": 1.5,
//   "motion_min_interval_ms": 200, "motion_max_interval_ms": 800
// }
typedef struct {
    uint8_t  hr_very_low;
    uint8_t  hr_low;
    uint8_t  hr_normal;
    uint8_t  hr_high;
    uint8_t  hr_very_high;
    uint8_t  spo2_very_low;
    uint8_t  spo2_low;
    uint8_t  spo2_normal;
    float    motion_threshold;
    uint32_t motion_min_interval_ms;
    uint32_t motion_max_interval_ms;
} alert_config_t;

// Variáveis globais — acessíveis por qualquer módulo que incluir este header
extern sampling_config_t g_sampling_config;
extern alert_config_t    g_alert_config;

// Parseia JSON recebido do MQTT e atualiza a config global correspondente.
// payload NÃO precisa ser null-terminated; len é tratado internamente.
// Retorna true se o parse foi bem-sucedido.
bool config_parse_sampling   (const char *payload, int len);
bool config_parse_wifi       (const char *payload, int len);
bool config_parse_thresholds (const char *payload, int len);

#endif
