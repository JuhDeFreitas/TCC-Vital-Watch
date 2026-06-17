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
//   "hr_very_low": 40,  "hr_low": 50, "hr_normal": 100,
//   "hr_high": 130,     "hr_very_high": 160,
//   "hr_running_high": 170, "hr_running_very_high": 200,   ← limites em corrida
//   "spo2_very_low": 90, "spo2_low": 94, "spo2_normal": 100,
//   "motion_threshold": 1.5,
//   "motion_min_interval_ms": 200, "motion_max_interval_ms": 800
// }
typedef struct {
    /* Frequência cardíaca — repouso
     * < 50  muito baixa | 50-59 baixa | 60-100 normal | 101-120 alta | > 120 muito alta */
    uint8_t  hr_very_low;      // < este valor → muito baixa
    uint8_t  hr_low;           // < este valor → baixa
    uint8_t  hr_high;          // >= este valor → alta
    uint8_t  hr_very_high;     // >= este valor → muito alta

    /* Frequência cardíaca — corrida
     * < 100  muito baixa | 100-109 baixa | 110-149 normal | 150-170 alta | > 170 muito alta */
    uint8_t  hr_running_very_low;    // < este valor → muito baixa em corrida
    uint8_t  hr_running_low;         // < este valor → baixa em corrida
    uint8_t  hr_running_high;        // >= este valor → alta em corrida
    uint8_t  hr_running_very_high;   // >= este valor → muito alta em corrida

    /* SpO2 */
    uint8_t  spo2_very_low;
    uint8_t  spo2_low;
    uint8_t  spo2_normal;

    /* Movimento */
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
