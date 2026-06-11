#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "i2c_api.h"
#include "max30102_api.h"
#include "algorithm.h"

static const char *TAG = "MAX30102";

// -------- Buffers de dados --------
static int32_t ir_buffer[BUFFER_SIZE];
static int32_t red_buffer[BUFFER_SIZE];
static double  autocorr_data[125];

// -------- Configuração do sensor --------
// Hardware: 100 Hz | SMP_AVE=4 → saída efetiva 25 Hz (40 ms/amostra)
// Modo SpO2: LED1 RED + LED2 IR
static max_config sensor_cfg = {

    .INT_EN_1.A_FULL_EN         = 1,
    .INT_EN_1.PPG_RDY_EN        = 1,
    .INT_EN_1.ALC_OVF_EN        = 0,
    .INT_EN_1.PROX_INT_EN       = 0,

    .INT_EN_2.DIE_TEMP_RDY_EN   = 0,

    .FIFO_WRITE_PTR.FIFO_WR_PTR = 0,
    .OVEF_COUNTER.OVF_COUNTER   = 0,
    .FIFO_READ_PTR.FIFO_RD_PTR  = 0,

    .FIFO_CONF.SMP_AVE          = 0b010,   // média de 4 → 25 Hz efetivo
    .FIFO_CONF.FIFO_ROLLOVER_EN = 1,
    .FIFO_CONF.FIFO_A_FULL      = 0,

    .MODE_CONF.SHDN             = 0,
    .MODE_CONF.RESET            = 0,
    .MODE_CONF.MODE             = 0b011,   // SpO2 mode (RED + IR)

    .SPO2_CONF.SPO2_ADC_RGE     = 0b01,   // 4096 nA
    .SPO2_CONF.SPO2_SR          = 0b001,  // 100 Hz hardware
    .SPO2_CONF.LED_PW           = 0b10,   // 215 µs (17 bits resolução)

    .LED1_PULSE_AMP.LED1_PA     = 0x5F,   // ~19 mA
    .LED2_PULSE_AMP.LED2_PA     = 0x5F,

    .PROX_LED_PULS_AMP.PILOT_PA = 0x7F,

    .MULTI_LED_CONTROL1.SLOT1   = 0,
    .MULTI_LED_CONTROL1.SLOT2   = 0,
    .MULTI_LED_CONTROL2.SLOT3   = 0,
    .MULTI_LED_CONTROL2.SLOT4   = 0,
};

// Coleta BUFFER_SIZE amostras do sensor com intervalo de DELAY_AMOSTRAGEM ms
static void fill_buffers_data(void)
{
    for (int i = 0; i < BUFFER_SIZE; i++) {
        ir_buffer[i]  = 0;
        red_buffer[i] = 0;
        read_max30102_fifo(&red_buffer[i], &ir_buffer[i]);
        vTaskDelay(pdMS_TO_TICKS(DELAY_AMOSTRAGEM));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando I2C...");
    ESP_ERROR_CHECK(i2c_init());
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Inicializando MAX30102...");
    max30102_init(&sensor_cfg);
    vTaskDelay(pdMS_TO_TICKS(500));

    init_time_array();

    ESP_LOGI(TAG, "Sensor pronto. Coloque o dedo firmemente no sensor.");
    ESP_LOGI(TAG, "Coletando %d amostras a cada ciclo (%.1f s)...",
             BUFFER_SIZE, BUFFER_SIZE * DELAY_AMOSTRAGEM / 1000.0f);

    while (1) {
        // 1. Coleta 128 amostras (~5.12 s)
        fill_buffers_data();

        // Verifica se há sinal (IR DC deve ser > 50.000 para dedo presente)
        int32_t ir_dc_check = 0;
        for (int i = 0; i < BUFFER_SIZE; i++) ir_dc_check += ir_buffer[i];
        ir_dc_check /= BUFFER_SIZE;

        if (ir_dc_check < 50000) {
            ESP_LOGW(TAG, "Sem dedo detectado (IR medio = %ld). Aguardando...", ir_dc_check);
            continue;
        }

        // 2. Processamento do sinal
        uint64_t ir_mean  = 0;
        uint64_t red_mean = 0;

        remove_dc_part(ir_buffer, red_buffer, &ir_mean, &red_mean);
        remove_trend_line(ir_buffer);
        remove_trend_line(red_buffer);

        // 3. Correlação de Pearson entre RED e IR
        double correlation = correlation_datay_datax(red_buffer, ir_buffer);

        // 4. Frequência cardíaca via autocorrelação
        double r0   = 0.0;
        int    bpm  = calculate_heart_rate(ir_buffer, &r0, autocorr_data);

        // 5. SpO2 apenas se correlação >= 0.7 (sinal de qualidade)
        double spo2 = -1.0;
        if (correlation >= 0.7) {
            spo2 = spo2_measurement(ir_buffer, red_buffer, ir_mean, red_mean);
        }

        // 6. Exibição
        printf("\n=========================================\n");
        printf("  Frequencia Cardiaca : ");
        if (bpm > 0 && bpm <= 220) {
            printf("%d BPM\n", bpm);
        } else {
            printf("N/A (sinal insuficiente)\n");
        }

        printf("  SpO2                : ");
        if (correlation >= 0.7 && spo2 >= 85.0 && spo2 <= 100.0) {
            printf("%.1f%%\n", spo2);
        } else if (correlation < 0.7) {
            printf("N/A (correlacao baixa: %.2f)\n", correlation);
        } else {
            printf("N/A (valor fora de faixa: %.1f)\n", spo2);
        }

        printf("  Correlacao RED/IR   : %.3f\n", correlation);
        printf("  DC IR               : %llu  |  DC RED: %llu\n", ir_mean, red_mean);
        printf("=========================================\n");
    }
}
