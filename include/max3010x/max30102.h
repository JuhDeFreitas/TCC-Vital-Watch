#ifndef MAX30102_H
#define MAX30102_H

#include "esp_err.h"
#include <stdint.h>

#define MAX30102_SAMPLE_RATE_HZ   100
#define MAX30102_BUFFER_SIZE      150   // 3 s @ 100 Hz
#define MAX30102_MAX_PEAKS        15

typedef struct {
    float heart_rate_bpm;
    float spo2_percent;
    bool hr_valid;
    bool spo2_valid;
} max30102_metrics_t;

/*
 * Inicialização do sensor MAX30102 e leitura dos valores brutos de RED e IR.
 */
esp_err_t max30102_init(void);
esp_err_t max30102_read_sample(uint32_t *red, uint32_t *ir);
//void start_max30102(void);

/**
 * Coleta uma janela completa de amostras chamando max30102_read_sample().
 */
esp_err_t max30102_collect_window(uint32_t *red_buf, uint32_t *ir_buf, int len);

/**
 * Processa os buffers e calcula BPM + SpO2.
 */
esp_err_t max30102_compute_metrics(const uint32_t *red_buf,
                                   const uint32_t *ir_buf,
                                   int len,
                                   float sample_rate_hz,
                                   max30102_metrics_t *out);

void max30102_task(void *pvParameters);
#endif