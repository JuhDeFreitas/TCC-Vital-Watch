/**
 * @file examples.c
 * @brief Exemplos de uso do MAX30102
 * @note Snippets para referência - NÃO É UM ARQUIVO COMPLETO
 */

#include "max30102_config.h"
#include "max30102_utils.h"

// ==================== EXEMPLO 1: VALIDAÇÃO BÁSICA ====================

/**
 * Exemplo: Validar resultados do sensor
 */
void example_validate_results(int32_t spo2, int8_t spo2_valid, 
                              int32_t hr, int8_t hr_valid) {
    
    printf("\n=== VALIDAÇÃO DE DADOS ===\n");
    
    if (spo2_valid && is_spo2_valid(spo2)) {
        printf("SpO2: %ld%% %s - %s\n", spo2, get_status_symbol(spo2_valid), 
               classify_spo2(spo2));
    } else {
        printf("SpO2: Inválido %s\n", get_status_symbol(spo2_valid));
    }
    
    if (hr_valid && is_hr_valid(hr)) {
        printf("HR: %ld BPM %s - %s\n", hr, get_status_symbol(hr_valid), 
               classify_hr(hr));
    } else {
        printf("HR: Inválido %s\n", get_status_symbol(hr_valid));
    }
}

// ==================== EXEMPLO 2: ANÁLISE ESTATÍSTICA ====================

/**
 * Exemplo: Analisar qualidade do sinal
 */
void example_signal_quality(uint32_t *ir_buffer, uint32_t *red_buffer, 
                           uint16_t buffer_size) {
    
    uint32_t ir_mean = calculate_mean(ir_buffer, buffer_size);
    uint32_t red_mean = calculate_mean(red_buffer, buffer_size);
    
    uint32_t ir_stddev = calculate_stddev(ir_buffer, buffer_size, ir_mean);
    uint32_t red_stddev = calculate_stddev(red_buffer, buffer_size, red_mean);
    
    // Calcular proporção de sinal AC/DC
    float ir_ac_ratio = (float)ir_stddev / (float)ir_mean;
    float red_ac_ratio = (float)red_stddev / (float)red_mean;
    
    // Estimar qualidade do sinal (0-100%)
    uint8_t signal_quality = (uint8_t)((ir_ac_ratio + red_ac_ratio) * 500);
    if (signal_quality > 100) signal_quality = 100;
    
    printf("\n=== QUALIDADE DO SINAL ===\n");
    printf("IR  Mean: %lu, StdDev: %lu, AC/DC: %.2f%%\n", 
           ir_mean, ir_stddev, ir_ac_ratio * 100);
    printf("RED Mean: %lu, StdDev: %lu, AC/DC: %.2f%%\n", 
           red_mean, red_stddev, red_ac_ratio * 100);
    printf("Qualidade: %d%% %s\n", signal_quality, 
           get_signal_bar(signal_quality));
}

// ==================== EXEMPLO 3: DETECÇÃO DE PICOS ====================

/**
 * Exemplo: Contar picos para verificar batidas
 */
void example_peak_detection(uint32_t *ir_buffer, uint16_t buffer_size) {
    
    uint32_t peak_count = 0;
    uint32_t threshold = calculate_mean(ir_buffer, buffer_size) / 10; // 10% da média
    
    // Detectar picos (deixar 1 amostra nas pontas)
    for (uint16_t i = 1; i < buffer_size - 1; i++) {
        if (is_peak(ir_buffer[i-1], ir_buffer[i], ir_buffer[i+1], threshold)) {
            peak_count++;
        }
    }
    
    // Estimar HR baseado em contagem de picos
    // 100 amostras = 4 segundos a 25Hz
    uint32_t estimated_hr = (peak_count * 60) / 4; // Converter para BPM
    
    printf("\n=== DETECÇÃO DE PICOS ===\n");
    printf("Picos detectados: %lu\n", peak_count);
    printf("HR Estimado: %lu BPM\n", estimated_hr);
    printf("Status: %s\n", is_hr_valid(estimated_hr) ? "VÁLIDO" : "INVÁLIDO");
}

// ==================== EXEMPLO 4: MONITORAMENTO CONTÍNUO ====================

/**
 * Exemplo: Loop com suavização de dados
 */
void example_continuous_monitoring(void) {
    
    static float smooth_spo2 = 95.0f;
    static float smooth_hr = 70.0f;
    static float smoothing_factor = 0.1f; // 10% peso para novo valor
    
    int32_t current_spo2 = 97;  // Valor do sensor
    int32_t current_hr = 72;
    
    // Suavizar valores para reduzir oscilações
    smooth_spo2 = smooth_value((float)current_spo2, smooth_spo2, smoothing_factor);
    smooth_hr = smooth_value((float)current_hr, smooth_hr, smoothing_factor);
    
    printf("SpO2 Bruto: %ld%% → Suavizado: %.1f%%\n", current_spo2, smooth_spo2);
    printf("HR Bruto: %ld BPM → Suavizado: %.1f BPM\n", current_hr, smooth_hr);
}

// ==================== EXEMPLO 5: LOGGING FORMATADO ====================

/**
 * Exemplo: Exibição formatada completa
 */
void example_formatted_output(int32_t spo2, int8_t spo2_valid,
                             int32_t hr, int8_t hr_valid,
                             uint32_t sample_count) {
    
    printf("\n");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║       MONITORAMENTO MAX30102 - STATUS      ║\n");
    printf("╠════════════════════════════════════════════╣\n");
    
    printf("║ Amostra: %lu                          ║\n", sample_count);
    printf("║                                            ║\n");
    
    printf("║ SpO2: %3ld%% [%s] %-24s ║\n", 
           spo2, 
           spo2_valid ? "OK" : "XX",
           classify_spo2(spo2));
    
    printf("║ HR:   %3ld BPM [%s] %-20s ║\n",
           hr,
           hr_valid ? "OK" : "XX",
           classify_hr(hr));
    
    printf("║                                            ║\n");
    
    if (spo2_valid && hr_valid) {
        printf("║ STATUS: ✓ MEDIÇÃO VÁLIDA                   ║\n");
    } else if (spo2_valid || hr_valid) {
        printf("║ STATUS: ⚠ MEDIÇÃO PARCIAL                 ║\n");
    } else {
        printf("║ STATUS: ✗ MEDIÇÃO INVÁLIDA                ║\n");
    }
    
    printf("╚════════════════════════════════════════════╝\n");
}

// ==================== EXEMPLO 6: CALIBRAÇÃO INTERATIVA ====================

/**
 * Exemplo: Ajuste de configurações
 */
void example_dynamic_configuration(void) {
    
    // Aumentar sensibilidade em ambiente com interferência
    const uint8_t LED_POWER_LOW = 0x0F;      // Potência baixa
    const uint8_t LED_POWER_MEDIUM = 0x1F;   // Potência média
    const uint8_t LED_POWER_HIGH = 0x3F;     // Potência alta
    
    uint8_t current_led_power = LED_POWER_MEDIUM;
    
    // Pseudocódigo de ajuste automático
    /*
    if (signal_quality < 30) {
        // Sinal muito fraco, aumentar potência
        current_led_power = LED_POWER_HIGH;
        printf("Aumentando potência do LED para melhorar sinal...\n");
    } else if (signal_quality > 80) {
        // Sinal muito forte, reduzir potência
        current_led_power = LED_POWER_LOW;
        printf("Reduzindo potência do LED...\n");
    }
    */
}

// ==================== EXEMPLO 7: DETECÇÃO DE ANOMALIAS ====================

/**
 * Exemplo: Detectar valores anormais
 */
typedef struct {
    int32_t min_spo2;
    int32_t max_spo2;
    int32_t min_hr;
    int32_t max_hr;
    uint32_t anomaly_count;
} health_thresholds_t;

void example_anomaly_detection(int32_t spo2, int32_t hr,
                              health_thresholds_t *thresholds) {
    
    bool spo2_anomaly = (spo2 < thresholds->min_spo2 || 
                         spo2 > thresholds->max_spo2);
    bool hr_anomaly = (hr < thresholds->min_hr || 
                       hr > thresholds->max_hr);
    
    if (spo2_anomaly || hr_anomaly) {
        thresholds->anomaly_count++;
        
        printf("\n⚠️  ANOMALIA DETECTADA:\n");
        
        if (spo2_anomaly) {
            printf("  - SpO2 fora do range: %ld%% (esperado: %ld-%ld%%)\n",
                   spo2, thresholds->min_spo2, thresholds->max_spo2);
        }
        
        if (hr_anomaly) {
            printf("  - HR fora do range: %ld BPM (esperado: %ld-%ld BPM)\n",
                   hr, thresholds->min_hr, thresholds->max_hr);
        }
        
        if (thresholds->anomaly_count > 5) {
            printf("  ⚠️  ALERTA: Múltiplas anomalias detectadas!\n");
        }
    }
}

// ==================== EXEMPLO 8: CONVERSÃO DE TEMPERATURA ====================

/**
 * Exemplo: Ler e converter temperatura do MAX30102
 */
void example_temperature_reading(uint8_t temp_int, uint8_t temp_frac) {
    
    float celsius = raw_to_celsius(temp_int, temp_frac);
    float fahrenheit = (celsius * 9.0f / 5.0f) + 32.0f;
    
    printf("\n=== TEMPERATURA DO SENSOR ===\n");
    printf("Temperatura: %.2f°C | %.2f°F\n", celsius, fahrenheit);
    
    // Verificar se está fora da faixa normal (±40°C)
    if (celsius < 15.0f || celsius > 40.0f) {
        printf("⚠️  Aviso: Temperatura fora da faixa operacional!\n");
    }
}

// ==================== EXEMPLO 9: RELATÓRIO RESUMIDO ====================

/**
 * Exemplo: Gerar relatório de sessão
 */
void example_session_report(int32_t spo2, int32_t hr, 
                           uint32_t total_samples) {
    
    printf("\n");
    printf("═══════════════════════════════════════════════\n");
    printf("         RELATÓRIO DE SESSÃO - MAX30102        \n");
    printf("═══════════════════════════════════════════════\n");
    printf("Timestamp: [timestamp]\n");
    printf("Total de amostras: %lu\n", total_samples);
    printf("Duração: ~%lu segundos\n", total_samples / 25);
    printf("\nMÉDICAS:\n");
    printf("  SpO2 Final: %ld%%\n", spo2);
    printf("  HR Final: %ld BPM\n", hr);
    printf("\nCLASSIFICAÇÃO:\n");
    printf("  SpO2: %s\n", classify_spo2(spo2));
    printf("  HR: %s\n", classify_hr(hr));
    printf("═══════════════════════════════════════════════\n");
}

/*
 * NOTAS DE USO:
 * 
 * 1. Estes são SNIPPETS de exemplo para referência
 * 2. O arquivo main.c contém a implementação completa
 * 3. Use as funções em max30102_utils.h para seus próprios dados
 * 4. Adapte conforme necessário para sua aplicação específica
 * 
 * Exemplo de compilação com headers:
 *   #include "max30102_config.h"
 *   #include "max30102_utils.h"
 */
