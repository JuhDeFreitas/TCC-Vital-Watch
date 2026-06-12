#include <stdio.h>
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

    .FIFO_CONF.SMP_AVE          = 0b010,   // média de 4 → 25 Hz efetivo
    .FIFO_CONF.FIFO_ROLLOVER_EN = 1,
    .FIFO_CONF.FIFO_A_FULL      = 0,

    .MODE_CONF.SHDN             = 0,
    .MODE_CONF.RESET            = 0,
    .MODE_CONF.MODE             = 0b011,   // SpO2 mode (RED + IR)

    .SPO2_CONF.SPO2_ADC_RGE     = 0b01,   // 4096 nA — range padrao (mais sensivel)
    .SPO2_CONF.SPO2_SR          = 0b001,  // 100 Hz hardware
    .SPO2_CONF.LED_PW           = 0b11,   // 411 µs — 18-bit

    .LED1_PULSE_AMP.LED1_PA     = 0x3F,   // ~12.5 mA RED
    .LED2_PULSE_AMP.LED2_PA     = 0x7F,   // ~25.4 mA IR

    .PROX_LED_PULS_AMP.PILOT_PA = 0x7F,

    .MULTI_LED_CONTROL1.SLOT1   = 0,
    .MULTI_LED_CONTROL1.SLOT2   = 0,
    .MULTI_LED_CONTROL2.SLOT3   = 0,
    .MULTI_LED_CONTROL2.SLOT4   = 0,
};

void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando I2C...");
    ESP_ERROR_CHECK(i2c_init());
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Inicializando MAX30102...");
    max30102_init(&sensor_cfg);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Limpa FIFO antes de começar
    write_max30102_reg(0, REG_FIFO_WR_PTR);
    write_max30102_reg(0, REG_OVF_COUNTER);
    write_max30102_reg(0, REG_FIFO_RD_PTR);

    ESP_LOGI(TAG, "Pronto. Coloque o dedo e observe o CSV abaixo.");
    ESP_LOGI(TAG, "Formato: amostra,IR,RED");

    int32_t ir, red;

    while (1) {
        ir  = 0;
        red = 0;
        read_max30102_fifo(&red, &ir);

        // Formato reconhecido pelo Serial Plotter do PlatformIO/VS Code
        printf("IR:%ld,RED:%ld\n", ir, red);

        vTaskDelay(pdMS_TO_TICKS(DELAY_AMOSTRAGEM));
    }
}
