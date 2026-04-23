#include <stdio.h>
#include <string.h>
#include <math.h>

#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c/i2c_config.h"
#include "max3010x/max30102.h"

static const char *TAG = "MAX30102";

// Endereço I2C do MAX30102
#define MAX30102_ADDR       0x57

// Registradores
#define REG_INTR_STATUS_1   0x00
#define REG_INTR_STATUS_2   0x01
#define REG_INTR_ENABLE_1   0x02
#define REG_INTR_ENABLE_2   0x03
#define REG_FIFO_WR_PTR     0x04
#define REG_OVF_COUNTER     0x05
#define REG_FIFO_RD_PTR     0x06
#define REG_FIFO_DATA       0x07
#define REG_FIFO_CONFIG     0x08
#define REG_MODE_CONFIG     0x09
#define REG_SPO2_CONFIG     0x0A
#define REG_LED1_PA         0x0C   // RED
#define REG_LED2_PA         0x0D   // IR
#define REG_PART_ID         0xFF

#define I2C_TIMEOUT_MS      1000

/*  Read Data from MAX30102 */

static esp_err_t max30102_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };

    return i2c_master_write_to_device(
        I2C_MASTER_NUM,
        MAX30102_ADDR,
        data,
        sizeof(data),
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
}

static esp_err_t max30102_read_reg(uint8_t reg, uint8_t *value)
{
    return i2c_master_write_read_device(
        I2C_MASTER_NUM,
        MAX30102_ADDR,
        &reg,
        1,
        value,
        1,
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
}

static esp_err_t max30102_read_multi(uint8_t reg, uint8_t *buffer, size_t len)
{
    return i2c_master_write_read_device(
        I2C_MASTER_NUM,
        MAX30102_ADDR,
        &reg,
        1,
        buffer,
        len,
        pdMS_TO_TICKS(I2C_TIMEOUT_MS)
    );
}

esp_err_t max30102_init(void)
{
    esp_err_t err;
    uint8_t part_id = 0;

    err = max30102_read_reg(REG_PART_ID, &part_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao ler PART_ID");
        return err;
    }

    ESP_LOGI(TAG, "PART_ID = 0x%02X", part_id);

    // Reset do sensor
    err = max30102_write_reg(REG_MODE_CONFIG, 0x40);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao resetar MAX30102");
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    // Desabilita interrupções
    err = max30102_write_reg(REG_INTR_ENABLE_1, 0x00);
    if (err != ESP_OK) return err;

    err = max30102_write_reg(REG_INTR_ENABLE_2, 0x00);
    if (err != ESP_OK) return err;

    // Zera FIFO
    err = max30102_write_reg(REG_FIFO_WR_PTR, 0x00);
    if (err != ESP_OK) return err;

    err = max30102_write_reg(REG_OVF_COUNTER, 0x00);
    if (err != ESP_OK) return err;

    err = max30102_write_reg(REG_FIFO_RD_PTR, 0x00);
    if (err != ESP_OK) return err;

    // FIFO_CONFIG
    // sample avg = 4
    // rollover enable
    // almost full = 0x0F
    err = max30102_write_reg(REG_FIFO_CONFIG, 0x5F);
    if (err != ESP_OK) return err;

    // SPO2_CONFIG
    // ADC range = 4096nA
    // sample rate = 100Hz
    // pulse width = 411us / 18-bit
    err = max30102_write_reg(REG_SPO2_CONFIG, 0x27);
    if (err != ESP_OK) return err;

    // Corrente dos LEDs
    err = max30102_write_reg(REG_LED1_PA, 0x24); // RED
    if (err != ESP_OK) return err;

    err = max30102_write_reg(REG_LED2_PA, 0x24); // IR
    if (err != ESP_OK) return err;

    // Modo SpO2
    err = max30102_write_reg(REG_MODE_CONFIG, 0x03);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "MAX30102 inicializado com sucesso");
    return ESP_OK;
}

esp_err_t max30102_read_sample(uint32_t *red, uint32_t *ir)
{
    uint8_t raw[6];
    esp_err_t err;

    if (red == NULL || ir == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    err = max30102_read_multi(REG_FIFO_DATA, raw, 6);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao ler FIFO");
        return err;
    }

    // 3 bytes RED + 3 bytes IR
    *red = ((uint32_t)raw[0] << 16) |
           ((uint32_t)raw[1] << 8)  |
           ((uint32_t)raw[2]);

    *ir  = ((uint32_t)raw[3] << 16) |
           ((uint32_t)raw[4] << 8)  |
           ((uint32_t)raw[5]);

    // MAX30102 usa 18 bits válidos
    *red &= 0x3FFFF;
    *ir  &= 0x3FFFF;

    return ESP_OK;
}


/* Conversion Functions for SPO2 and Heart Rate */

void process_data(uint32_t *red, uint32_t *ir, int size)
{
    float dc_red = 0, dc_ir = 0;
    float max_red = 0, min_red = 1e9;
    float max_ir = 0, min_ir = 1e9;

    for (int i = 0; i < size; i++)
    {
        dc_red += red[i];
        dc_ir  += ir[i];

        if (red[i] > max_red) max_red = red[i];
        if (red[i] < min_red) min_red = red[i];

        if (ir[i] > max_ir) max_ir = ir[i];
        if (ir[i] < min_ir) min_ir = ir[i];
    }

    dc_red /= size;
    dc_ir  /= size;

    float ac_red = max_red - min_red;
    float ac_ir  = max_ir - min_ir;

    float R = (ac_red / dc_red) / (ac_ir / dc_ir);
    float spo2 = 110.0f - 25.0f * R;

    printf("SpO2: %.2f%%\n", spo2);
}

/* =========================
 * Funções auxiliares internas
 * ========================= */

static float mean_u32(const uint32_t *x, int len)
{
    if (x == NULL || len <= 0) return 0.0f;

    double sum = 0.0;
    for (int i = 0; i < len; i++) {
        sum += (double)x[i];
    }
    return (float)(sum / len);
}

static float clampf_local(float x, float min_v, float max_v)
{
    if (x < min_v) return min_v;
    if (x > max_v) return max_v;
    return x;
}

/**
 * Remove DC do IR:
 *   y[i] = mean(IR) - IR[i]
 *
 * Isso "inverte" o sinal para aproximar a lógica usada em algoritmos
 * clássicos do MAX3010x, que detectam vales usando detector de picos
 * após inverter o sinal.
 */
static void ir_remove_dc_and_invert(const uint32_t *ir_buf, int len, float *out)
{
    float dc = mean_u32(ir_buf, len);

    for (int i = 0; i < len; i++) {
        out[i] = dc - (float)ir_buf[i];
    }
}

/**
 * Média móvel simples.
 */
static void moving_average(const float *in, float *out, int len, int win)
{
    if (win <= 1) {
        for (int i = 0; i < len; i++) out[i] = in[i];
        return;
    }

    for (int i = 0; i < len; i++) {
        float sum = 0.0f;
        int count = 0;

        int start = i - win / 2;
        int end   = i + win / 2;

        if (start < 0) start = 0;
        if (end >= len) end = len - 1;

        for (int j = start; j <= end; j++) {
            sum += in[j];
            count++;
        }

        out[i] = (count > 0) ? (sum / count) : in[i];
    }
}

/**
 * Detecta picos locais acima de um limiar, com distância mínima entre eles.
 */
static int detect_peaks(const float *x,
                        int len,
                        float threshold,
                        int min_distance,
                        int *peak_idx,
                        int max_peaks)
{
    int n = 0;
    int last_peak = -min_distance - 1;

    for (int i = 1; i < len - 1; i++) {
        bool is_peak = (x[i] > x[i - 1]) &&
                       (x[i] >= x[i + 1]) &&
                       (x[i] > threshold);

        if (!is_peak) {
            continue;
        }

        if ((i - last_peak) < min_distance) {
            // Se estiver muito perto, mantém o maior
            if (n > 0 && x[i] > x[peak_idx[n - 1]]) {
                peak_idx[n - 1] = i;
                last_peak = i;
            }
            continue;
        }

        if (n < max_peaks) {
            peak_idx[n++] = i;
            last_peak = i;
        } else {
            break;
        }
    }

    return n;
}

static float mean_float(const float *x, int len)
{
    if (x == NULL || len <= 0) return 0.0f;

    float sum = 0.0f;
    for (int i = 0; i < len; i++) {
        sum += x[i];
    }
    return sum / len;
}

/**
 * Cálculo simples da amplitude AC em um trecho:
 *   AC = max - min
 * e DC = média do trecho
 *
 * É uma versão didática e razoavelmente robusta para protótipo.
 * Implementações mais clássicas do ecossistema MAX3010x fazem a remoção
 * do DC por interpolação entre vales e depois calculam a razão por batida.
 */
static bool compute_ac_dc_segment(const uint32_t *buf,
                                  int start,
                                  int end,
                                  float *ac,
                                  float *dc)
{
    if (buf == NULL || ac == NULL || dc == NULL) return false;
    if (start < 0 || end <= start) return false;

    uint32_t min_v = UINT32_MAX;
    uint32_t max_v = 0;
    double sum = 0.0;
    int count = 0;

    for (int i = start; i <= end; i++) {
        uint32_t v = buf[i];
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
        sum += (double)v;
        count++;
    }

    if (count <= 1) return false;

    *ac = (float)(max_v - min_v);
    *dc = (float)(sum / count);

    return (*dc > 1.0f);
}

esp_err_t max30102_collect_window(uint32_t *red_buf, uint32_t *ir_buf, int len)
{
    if (red_buf == NULL || ir_buf == NULL || len <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    TickType_t delay_ticks = pdMS_TO_TICKS(1000 / MAX30102_SAMPLE_RATE_HZ);
    if (delay_ticks == 0) {
        delay_ticks = 1;
    }

    for (int i = 0; i < len; i++) {
        esp_err_t err = max30102_read_sample(&red_buf[i], &ir_buf[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Falha ao ler amostra %d", i);
            return err;
        }

        vTaskDelay(delay_ticks);
    }

    return ESP_OK;
}

esp_err_t max30102_compute_metrics(const uint32_t *red_buf,
                                   const uint32_t *ir_buf,
                                   int len,
                                   float sample_rate_hz,
                                   max30102_metrics_t *out)

{
    if (red_buf == NULL || ir_buf == NULL || out == NULL || len < 50 || sample_rate_hz <= 1.0f) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    static float ir_dc_removed[MAX30102_BUFFER_SIZE];
    static float ir_filtered[MAX30102_BUFFER_SIZE];

    if (len > MAX30102_BUFFER_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    // 1) Remove DC do IR e suaviza
    ir_remove_dc_and_invert(ir_buf, len, ir_dc_removed);
    moving_average(ir_dc_removed, ir_filtered, len, 4);

    // 2) Limiar simples para pico: metade da média dos valores positivos
    float positive_sum = 0.0f;
    int positive_count = 0;
    for (int i = 0; i < len; i++) {
        if (ir_filtered[i] > 0.0f) {
            positive_sum += ir_filtered[i];
            positive_count++;
        }
    }

    float avg_positive = (positive_count > 0) ? (positive_sum / positive_count) : 0.0f;
    float threshold = avg_positive * 0.5f;

    // Distância mínima entre picos:
    // 0.4 s => até 150 BPM
    int min_distance = (int)(0.4f * sample_rate_hz);
    if (min_distance < 1) min_distance = 1;

    int peaks[MAX30102_MAX_PEAKS] = {0};
    int n_peaks = detect_peaks(ir_filtered, len, threshold, min_distance, peaks, MAX30102_MAX_PEAKS);

    if (n_peaks >= 2) {
        float interval_sum = 0.0f;
        int interval_count = 0;

        for (int i = 1; i < n_peaks; i++) {
            int delta = peaks[i] - peaks[i - 1];
            if (delta > 0) {
                interval_sum += (float)delta;
                interval_count++;
            }
        }

        if (interval_count > 0) {
            float avg_interval_samples = interval_sum / interval_count;
            out->heart_rate_bpm = 60.0f * sample_rate_hz / avg_interval_samples;

            // Faixa plausível
            out->hr_valid = (out->heart_rate_bpm >= 35.0f && out->heart_rate_bpm <= 220.0f);
        }
    }

    // 3) SpO2 por segmentos entre picos consecutivos
    float r_values[10] = {0};
    int r_count = 0;

    for (int i = 0; i < n_peaks - 1 && r_count < 10; i++) {
        int start = peaks[i];
        int end   = peaks[i + 1];

        if ((end - start) < 3) {
            continue;
        }

        float ac_red = 0.0f, dc_red = 0.0f;
        float ac_ir  = 0.0f, dc_ir  = 0.0f;

        bool ok_red = compute_ac_dc_segment(red_buf, start, end, &ac_red, &dc_red);
        bool ok_ir  = compute_ac_dc_segment(ir_buf,  start, end, &ac_ir,  &dc_ir);

        if (!ok_red || !ok_ir) {
            continue;
        }

        if (ac_red <= 0.0f || ac_ir <= 0.0f) {
            continue;
        }

        float r = (ac_red / dc_red) / (ac_ir / dc_ir);

        // rejeita valores absurdos
        if (r > 0.2f && r < 4.0f) {
            r_values[r_count++] = r;
        }
    }

    if (r_count > 0) {
        float r_mean = mean_float(r_values, r_count);

        // aproximação linear mostrada pela ADI:
        // SpO2 = 104 - 17R
        out->spo2_percent = 104.0f - 17.0f * r_mean;
        out->spo2_percent = clampf_local(out->spo2_percent, 0.0f, 100.0f);

        // faixa plausível para protótipo
        out->spo2_valid = (out->spo2_percent >= 70.0f && out->spo2_percent <= 100.0f);
    }

    ESP_LOGI(TAG,
             "Peaks=%d HR=%.1f(valid=%d) SpO2=%.1f(valid=%d)",
             n_peaks,
             out->heart_rate_bpm,
             out->hr_valid,
             out->spo2_percent,
             out->spo2_valid);

    return ESP_OK;
}



void max30102_task(void *pvParameters)
{
    static uint32_t red_buf[MAX30102_BUFFER_SIZE];
    static uint32_t ir_buf[MAX30102_BUFFER_SIZE];

    while (1) {
        /* Coletando amostra de dados para o buffer */
        esp_err_t err = max30102_collect_window(red_buf, ir_buf, MAX30102_BUFFER_SIZE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Erro ao coletar janela");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        max30102_metrics_t metrics = {0};

        /* Caculando as métricas a partir das amostras coletadas. */
        err = max30102_compute_metrics(red_buf,
                                       ir_buf,
                                       MAX30102_BUFFER_SIZE,
                                       MAX30102_SAMPLE_RATE_HZ,
                                       &metrics);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Erro ao processar métricas");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        /* Mostrando metricas */
        if (metrics.hr_valid) {
            ESP_LOGI(TAG, "Heart Rate: %.1f BPM", metrics.heart_rate_bpm);
        } else {
            ESP_LOGW(TAG, "Heart Rate inválido");
        }

        if (metrics.spo2_valid) {
            ESP_LOGI(TAG, "SpO2: %.1f %%", metrics.spo2_percent);
        } else {
            ESP_LOGW(TAG, "SpO2 inválido");
        }


        /* Pulbicando metricas via mqtt*/
        mqtt_publish_message("");
        ESP_LOGI(TAG, "Message published");
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
