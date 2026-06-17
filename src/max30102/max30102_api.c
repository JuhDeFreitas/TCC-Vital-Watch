#include "max30102_api.h"
#include "algorithm.h"
#include "i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define TELEPLOT_ENABLED 0   // 0 para desativar

static const char *TAG = "MAX30102";

static max30102_result_cb_t s_result_cb = NULL;

// Configuração idêntica à do commit "FUNCIONOU!!!!! PERFEITO"
static const max_config s_default_cfg = {
    .INT_EN_1.A_FULL_EN         = 1,
    .INT_EN_1.PPG_RDY_EN        = 1,
    .INT_EN_1.ALC_OVF_EN        = 0,
    .INT_EN_1.PROX_INT_EN       = 0,

    .INT_EN_2.DIE_TEMP_RDY_EN   = 0,

    .FIFO_WRITE_PTR.FIFO_WR_PTR = 0,
    .OVEF_COUNTER.OVF_COUNTER   = 0,
    .FIFO_READ_PTR.FIFO_RD_PTR  = 0,

    .FIFO_CONF.SMP_AVE          = 0b010,   // 4 amostras → 25 Hz efetivo
    .FIFO_CONF.FIFO_ROLLOVER_EN = 1,
    .FIFO_CONF.FIFO_A_FULL      = 0,

    .MODE_CONF.SHDN             = 0,
    .MODE_CONF.RESET            = 0,
    .MODE_CONF.MODE             = 0b011,   // SpO2 mode (RED + IR)

    .SPO2_CONF.SPO2_ADC_RGE     = 0b01,   // 4096 nA
    .SPO2_CONF.SPO2_SR          = 0b001,  // 100 Hz hardware
    .SPO2_CONF.LED_PW           = 0b11,   // 411 µs — 18-bit

    .LED1_PULSE_AMP.LED1_PA     = 0x18,   // ~4.8 mA RED
    .LED2_PULSE_AMP.LED2_PA     = 0x18,   // ~4.8 mA IR

    .PROX_LED_PULS_AMP.PILOT_PA = 0x7F,

    .MULTI_LED_CONTROL1.SLOT1   = 0,
    .MULTI_LED_CONTROL1.SLOT2   = 0,
    .MULTI_LED_CONTROL2.SLOT3   = 0,
    .MULTI_LED_CONTROL2.SLOT4   = 0,
};

// ---- Task de medição -------------------------------------------------------

static void max30102_task(void *arg)
{
    static int32_t ir_buf[BUFFER_SIZE];
    static int32_t red_buf[BUFFER_SIZE];
    static double  auto_corr_buf[BUFFER_SIZE];

    while (1) {
        // Limpa FIFO antes de cada janela
        write_max30102_reg(0, REG_FIFO_WR_PTR);
        write_max30102_reg(0, REG_OVF_COUNTER);
        write_max30102_reg(0, REG_FIFO_RD_PTR);

        // Coleta BUFFER_SIZE amostras e envia para Teleplot em tempo real
        for (int i = 0; i < BUFFER_SIZE; i++) {
            read_max30102_fifo(&red_buf[i], &ir_buf[i]);
            #if TELEPLOT_ENABLED
                printf(">IR:%ld\n>RED:%ld\n", (long)ir_buf[i], (long)red_buf[i]);
            #endif
            //printf(">IR:%ld\n>RED:%ld\n", (long)ir_buf[i], (long)red_buf[i]);
            vTaskDelay(pdMS_TO_TICKS(DELAY_AMOSTRAGEM));
        }

        // Verifica presença do dedo pelo nível DC do IR
        uint64_t ir_sum = 0;
        for (int i = 0; i < BUFFER_SIZE; i++) ir_sum += (uint64_t)ir_buf[i];
        uint64_t ir_dc_check = ir_sum / BUFFER_SIZE;

        if (ir_dc_check < 10000) {
            ESP_LOGW(TAG, "Sem dedo (IR DC = %llu)", (unsigned long long)ir_dc_check);
            if (s_result_cb) s_result_cb(-1, -1.0);
            continue;
        }

        // Remove componente DC (salva médias para cálculo de SpO2)
        uint64_t ir_mean  = 0;
        uint64_t red_mean = 0;
        remove_dc_part(ir_buf, red_buf, &ir_mean, &red_mean);

        // Remove tendência linear (baseline drift)
        remove_trend_line(ir_buf);
        remove_trend_line(red_buf);

        // Correlação RED↔IR como indicador de qualidade do sinal
        double corr = correlation_datay_datax(red_buf, ir_buf);

        // Frequência cardíaca via autocorrelação do IR
        double r0 = 0.0;
        int hr = calculate_heart_rate(ir_buf, &r0, auto_corr_buf);

        // RMS dos canais AC
        double ir_rms  = rms_value(ir_buf);
        double red_rms = rms_value(red_buf);

        // Razão R (ideal ≥ 1.40 para SpO2 > 95%)
        double R_ratio = 0.0;
        if (ir_mean > 0 && red_mean > 0 && ir_rms > 0.0) {
            R_ratio = (red_rms / (double)red_mean) / (ir_rms / (double)ir_mean);
        }

        // SpO2 calculada somente com sinal forte E razão R fisiologicamente válida
        double spo2 = -1.0;
        if (r0 >= 500.0 && R_ratio >= 1.20 && corr >= 0.85) {
            spo2 = spo2_measurement(ir_buf, red_buf, ir_mean, red_mean);
        }

        // BPM fora da faixa fisiológica → inválido
        int bpm = (hr > 30 && hr < 220) ? hr : -1;

        ESP_LOGI(TAG, "DC: IR=%llu RED=%llu | AC_RMS: IR=%.1f RED=%.1f | R=%.3f corr=%.3f r0=%.1f",
                 (unsigned long long)ir_mean, (unsigned long long)red_mean,
                 ir_rms, red_rms, R_ratio, corr, r0);

        if (s_result_cb) s_result_cb(bpm, spo2);
    }
}

// ---- API pública -----------------------------------------------------------

esp_err_t max30102_init(max30102_result_cb_t on_result)
{
    s_result_cb = on_result;

    write_max30102_reg(s_default_cfg.data1,  REG_INTR_ENABLE_1);
    write_max30102_reg(s_default_cfg.data2,  REG_INTR_ENABLE_2);
    write_max30102_reg(s_default_cfg.data3,  REG_FIFO_WR_PTR);
    write_max30102_reg(s_default_cfg.data4,  REG_OVF_COUNTER);
    write_max30102_reg(s_default_cfg.data5,  REG_FIFO_RD_PTR);
    write_max30102_reg(s_default_cfg.data6,  REG_FIFO_CONFIG);
    write_max30102_reg(s_default_cfg.data7,  REG_MODE_CONFIG);
    write_max30102_reg(s_default_cfg.data8,  REG_SPO2_CONFIG);
    write_max30102_reg(s_default_cfg.data9,  REG_LED1_PA);
    write_max30102_reg(s_default_cfg.data10, REG_LED2_PA);
    write_max30102_reg(s_default_cfg.data11, REG_PILOT_PA);
    write_max30102_reg(s_default_cfg.data12, REG_MULTI_LED_CTRL1);
    write_max30102_reg(s_default_cfg.data13, REG_MULTI_LED_CTRL2);

    init_time_array();

    xTaskCreate(max30102_task, "max30102", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Inicializado (SpO2, 25 Hz efetivo, 18-bit ADC)");
    return ESP_OK;
}

// ---- Acesso de baixo nível -------------------------------------------------

void read_max30102_fifo(int32_t *red_data, int32_t *ir_data)
{
    uint8_t buf[6]   = {0};
    uint8_t fifo_reg = REG_FIFO_DATA;

    i2c_write_read(MAX30102_ADDR, &fifo_reg, 1, buf, 6);

    *red_data = ((int32_t)buf[0] << 16) | ((int32_t)buf[1] << 8) | buf[2];
    *ir_data  = ((int32_t)buf[3] << 16) | ((int32_t)buf[4] << 8) | buf[5];

    *red_data &= 0x3FFFF;
    *ir_data  &= 0x3FFFF;
}

void read_max30102_reg(uint8_t reg_addr, uint8_t *data_reg, size_t bytes_to_read)
{
    i2c_write_read(MAX30102_ADDR, &reg_addr, 1, data_reg, bytes_to_read);
}

void write_max30102_reg(uint8_t command, uint8_t reg)
{
    uint8_t data[2] = {reg, command};
    i2c_write(MAX30102_ADDR, data, 2);
}

float get_max30102_temp(void)
{
    uint8_t int_part  = 0;
    uint8_t frac_part = 0;
    write_max30102_reg(1, REG_TEMP_CONFIG);
    read_max30102_reg(REG_TEMP_INTR, &int_part,  1);
    read_max30102_reg(REG_TEMP_FRAC, &frac_part, 1);
    return (float)int_part + ((float)frac_part * 0.0625f);
}
