#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "i2c_api.h"
#include "max30102_api.h"
#include "algorithm.h"

static const char *TAG = "MAX30102";

static max_config sensor_cfg = {

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

// Buffers estáticos para evitar uso de stack
static int32_t ir_buf[BUFFER_SIZE];
static int32_t red_buf[BUFFER_SIZE];
static double  auto_corr_buf[BUFFER_SIZE];

void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando I2C...");
    ESP_ERROR_CHECK(i2c_init());
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Inicializando MAX30102...");
    max30102_init(&sensor_cfg);
    vTaskDelay(pdMS_TO_TICKS(500));

    init_time_array();

    ESP_LOGI(TAG, "Pronto. Coloque o dedo no sensor.");
    ESP_LOGI(TAG, "Coletando %d amostras por janela (~%.1f s)...",
             BUFFER_SIZE, BUFFER_SIZE * DELAY_AMOSTRAGEM / 1000.0f);

    while (1) {
        // Limpa FIFO antes de cada janela
        write_max30102_reg(0, REG_FIFO_WR_PTR);
        write_max30102_reg(0, REG_OVF_COUNTER);
        write_max30102_reg(0, REG_FIFO_RD_PTR);

        // Coleta BUFFER_SIZE amostras
        for (int i = 0; i < BUFFER_SIZE; i++) {
            read_max30102_fifo(&red_buf[i], &ir_buf[i]);
            vTaskDelay(pdMS_TO_TICKS(DELAY_AMOSTRAGEM));
        }

        // Verifica presença do dedo pelo nível DC do IR
        uint64_t ir_sum = 0;
        for (int i = 0; i < BUFFER_SIZE; i++) ir_sum += (uint64_t)ir_buf[i];
        uint64_t ir_dc_check = ir_sum / BUFFER_SIZE;

        if (ir_dc_check < 10000) {
            printf("--- Sem dedo (IR DC = %llu) ---\n",
                   (unsigned long long)ir_dc_check);
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

        // Calcula frequência cardíaca via autocorrelação do IR
        double r0 = 0.0;
        int hr = calculate_heart_rate(ir_buf, &r0, auto_corr_buf);

        // Calcula RMS do sinal AC para diagnóstico
        double ir_rms  = rms_value(ir_buf);
        double red_rms = rms_value(red_buf);

        // Razão R (diagnóstico — ideal: 0.5–0.7 para SpO2 > 95%)
        double R_ratio = 0.0;
        if (ir_mean > 0 && red_mean > 0 && ir_rms > 0.0) {
            R_ratio = (red_rms / (double)red_mean) / (ir_rms / (double)ir_mean);
        }

        // Calcula SpO2 somente quando sinal é forte E razão R é fisiologicamente válida
        // R_ratio >= 1.40 corresponde ao inverso do R ideal (0.50-0.70) da fórmula Maxim
        double spo2 = -1.0;
        if (r0 >= 500.0 && R_ratio >= 1.20 && corr >= 0.85)
            spo2 = spo2_measurement(ir_buf, red_buf, ir_mean, red_mean);

        // Exibe resultados
        printf("=== Resultado ===\n");

        if (hr > 30 && hr < 220) {
            printf("  FC  : %d bpm\n", hr);
        } else {
            printf("  FC  : N/A (sinal insuficiente)\n");
        }

        if (spo2 >= 70.0 && spo2 <= 100.0) {
            printf("  SpO2: %.1f %%\n", spo2);
        } else {
            printf("  SpO2: N/A (calculado: %.1f)\n", spo2);
        }

        printf("  DC  : IR=%llu  RED=%llu  (ideal: RED>=IR)\n",
               (unsigned long long)ir_mean,
               (unsigned long long)red_mean);
        printf("  AC  : IR_RMS=%.1f  RED_RMS=%.1f\n", ir_rms, red_rms);
        printf("  R   : %.3f  (ideal: >=1.40 para SpO2 > 95%%)\n", R_ratio);
        printf("  Corr: %.3f  r0=%.1f\n\n", corr, r0);
    }
}
