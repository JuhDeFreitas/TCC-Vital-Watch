/**
 * @file max30102_utils.h
 * @brief Funções utilitárias para MAX30102
 */

#ifndef MAX30102_UTILS_H
#define MAX30102_UTILS_H

#include <stdint.h>
#include <stdbool.h>

// ==================== VALIDAÇÃO DE DADOS ====================

/**
 * @brief Verifica se SpO2 está dentro do range válido
 * @param spo2 Valor de SpO2 em %
 * @return true se válido, false caso contrário
 */
static inline bool is_spo2_valid(int32_t spo2) {
    return (spo2 >= 50 && spo2 <= 100);
}

/**
 * @brief Verifica se Heart Rate está dentro do range válido
 * @param hr Valor de Heart Rate em BPM
 * @return true se válido, false caso contrário
 */
static inline bool is_hr_valid(int32_t hr) {
    return (hr >= 30 && hr <= 200);
}

/**
 * @brief Classifica nível de SpO2
 * @param spo2 Valor de SpO2 em %
 * @return Ponteiro para string descritiva
 */
static inline const char* classify_spo2(int32_t spo2) {
    if (spo2 >= 95) return "Excelente (≥95%)";
    if (spo2 >= 90) return "Normal (90-94%)";
    if (spo2 >= 85) return "Baixo (85-89%)";
    return "Crítico (<85%)";
}

/**
 * @brief Classifica nível de Heart Rate
 * @param hr Valor de Heart Rate em BPM
 * @return Ponteiro para string descritiva
 */
static inline const char* classify_hr(int32_t hr) {
    if (hr < 40) return "Muito Baixo (<40)";
    if (hr < 60) return "Bradicárdia (40-59)";
    if (hr <= 100) return "Normal (60-100)";
    if (hr <= 120) return "Elevado (101-120)";
    return "Muito Elevado (>120)";
}

// ==================== CONVERSÃO E CÁLCULO ====================

/**
 * @brief Calcula razão entre dois valores (evita divisão por zero)
 * @param numerador Valor do numerador
 * @param denominador Valor do denominador
 * @return Razão em ponto flutuante
 */
static inline float safe_ratio(uint32_t numerador, uint32_t denominador) {
    if (denominador == 0) return 0.0f;
    return (float)numerador / (float)denominador;
}

/**
 * @brief Suaviza valor usando média móvel simples
 * @param valor_novo Novo valor a ser adicionado
 * @param media_anterior Média anterior
 * @param peso Peso do novo valor (0.1 = 10%)
 * @return Novo valor de média
 */
static inline float smooth_value(float valor_novo, float media_anterior, float peso) {
    return media_anterior * (1.0f - peso) + valor_novo * peso;
}

/**
 * @brief Converte temperatura raw para Celsius
 * @param temp_int Byte inteiro da temperatura
 * @param temp_frac Byte fração da temperatura
 * @return Temperatura em Celsius (ponto flutuante)
 */
static inline float raw_to_celsius(uint8_t temp_int, uint8_t temp_frac) {
    return (float)temp_int + ((float)temp_frac * 0.0625f);
}

// ==================== ESTATÍSTICAS ====================

/**
 * @brief Calcula média de um array
 * @param array Ponteiro para array
 * @param length Número de elementos
 * @return Valor da média
 */
static inline uint32_t calculate_mean(uint32_t *array, uint16_t length) {
    if (length == 0) return 0;
    uint64_t sum = 0;
    for (uint16_t i = 0; i < length; i++) {
        sum += array[i];
    }
    return (uint32_t)(sum / length);
}

/**
 * @brief Calcula desvio padrão
 * @param array Ponteiro para array
 * @param length Número de elementos
 * @param media Média já calculada
 * @return Desvio padrão
 */
static inline uint32_t calculate_stddev(uint32_t *array, uint16_t length, uint32_t media) {
    if (length <= 1) return 0;
    
    uint64_t sum_squares = 0;
    for (uint16_t i = 0; i < length; i++) {
        int32_t diff = (int32_t)array[i] - (int32_t)media;
        sum_squares += diff * diff;
    }
    
    // Retorna approximação da raiz quadrada
    uint32_t variance = (uint32_t)(sum_squares / length);
    return (uint32_t)isqrt(variance);
}

/**
 * @brief Implementação simplificada de raiz quadrada inteira
 */
static inline uint32_t isqrt(uint32_t n) {
    if (n == 0) return 0;
    uint32_t x = n;
    uint32_t y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

// ==================== DETECÇÃO DE PICOS ====================

/**
 * @brief Detecta pico em três valores consecutivos
 * @param prev Valor anterior
 * @param curr Valor atual
 * @param next Próximo valor
 * @param threshold Limiar mínimo de diferença
 * @return true se é pico, false caso contrário
 */
static inline bool is_peak(uint32_t prev, uint32_t curr, uint32_t next, uint32_t threshold) {
    return (curr > prev && curr > next && (curr - prev >= threshold) && (curr - next >= threshold));
}

/**
 * @brief Detecta vale em três valores consecutivos
 * @param prev Valor anterior
 * @param curr Valor atual
 * @param next Próximo valor
 * @param threshold Limiar mínimo de diferença
 * @return true se é vale, false caso contrário
 */
static inline bool is_valley(uint32_t prev, uint32_t curr, uint32_t next, uint32_t threshold) {
    return (curr < prev && curr < next && (prev - curr >= threshold) && (next - curr >= threshold));
}

// ==================== FORMATOS E EXIBIÇÃO ====================

/**
 * @brief Retorna símbolo para força de sinal
 * @param signal Intensidade do sinal (0-100)
 * @return String com símbolo visual
 */
static inline const char* get_signal_bar(uint8_t signal) {
    if (signal >= 90) return "█████ (Excelente)";
    if (signal >= 70) return "████░ (Bom)";
    if (signal >= 50) return "███░░ (Aceitável)";
    if (signal >= 30) return "██░░░ (Fraco)";
    return "█░░░░ (Muito Fraco)";
}

/**
 * @brief Retorna emoji/símbolo de status
 * @param valid Flag de validade
 * @return String com símbolo
 */
static inline const char* get_status_symbol(int8_t valid) {
    return valid ? "✓" : "✗";
}

#endif // MAX30102_UTILS_H
