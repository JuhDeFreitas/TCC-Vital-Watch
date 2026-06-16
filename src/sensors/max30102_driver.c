#include "sensors/max30102_driver.h"
#include "i2c.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Register map */
#define REG_INTR_ENABLE_1   0x02
#define REG_INTR_ENABLE_2   0x03
#define REG_FIFO_WR_PTR     0x04
#define REG_OVF_COUNTER     0x05
#define REG_FIFO_RD_PTR     0x06
#define REG_FIFO_DATA       0x07
#define REG_FIFO_CONFIG     0x08
#define REG_MODE_CONFIG     0x09
#define REG_SPO2_CONFIG     0x0A
#define REG_LED1_PA         0x0C
#define REG_LED2_PA         0x0D
#define REG_PILOT_PA        0x10
#define REG_MULTI_LED_CTRL1 0x11
#define REG_MULTI_LED_CTRL2 0x12

static void write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    i2c_master_write_to_device(I2C_MASTER_NUM, MAX30102_ADDR,
                               data, sizeof(data), pdMS_TO_TICKS(100));
}

esp_err_t max30102_init(void)
{
    /* Interrupt enable: A_FULL | PPG_RDY */
    write_reg(REG_INTR_ENABLE_1, 0xC0);
    write_reg(REG_INTR_ENABLE_2, 0x00);

    /* Reset FIFO pointers */
    write_reg(REG_FIFO_WR_PTR,   0x00);
    write_reg(REG_OVF_COUNTER,   0x00);
    write_reg(REG_FIFO_RD_PTR,   0x00);

    /* FIFO config: SMP_AVE=4 (0b010<<5), ROLLOVER_EN=1 (bit4), A_FULL=0 */
    write_reg(REG_FIFO_CONFIG,   0x50);

    /* SpO2 mode */
    write_reg(REG_MODE_CONFIG,   0x03);

    /* SpO2 config: ADC_RGE=4096nA (0b01<<5), SR=100Hz (0b001<<2), LED_PW=411us (0b11) */
    write_reg(REG_SPO2_CONFIG,   0x27);

    /* LED pulse amplitude: ~4.8 mA each */
    write_reg(REG_LED1_PA,       0x18);
    write_reg(REG_LED2_PA,       0x18);
    write_reg(REG_PILOT_PA,      0x7F);

    write_reg(REG_MULTI_LED_CTRL1, 0x00);
    write_reg(REG_MULTI_LED_CTRL2, 0x00);

    return ESP_OK;
}

esp_err_t max30102_read_sample(uint32_t *red, uint32_t *ir)
{
    uint8_t buf[6] = {0};
    uint8_t reg    = REG_FIFO_DATA;

    esp_err_t err = i2c_master_write_read_device(
        I2C_MASTER_NUM, MAX30102_ADDR,
        &reg, 1,
        buf, 6,
        pdMS_TO_TICKS(1000)
    );

    if (err != ESP_OK) return err;

    /* Each channel: 3 bytes, 18 useful bits, MSB first */
    *red = (((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2]) & 0x3FFFF;
    *ir  = (((uint32_t)buf[3] << 16) | ((uint32_t)buf[4] << 8) | buf[5]) & 0x3FFFF;

    return ESP_OK;
}

esp_err_t max30102_collect_window(uint32_t *red_buf, uint32_t *ir_buf, int len)
{
    /* Flush FIFO before collection */
    write_reg(REG_FIFO_WR_PTR, 0x00);
    write_reg(REG_OVF_COUNTER, 0x00);
    write_reg(REG_FIFO_RD_PTR, 0x00);

    for (int i = 0; i < len; i++) {
        esp_err_t err = max30102_read_sample(&red_buf[i], &ir_buf[i]);
        if (err != ESP_OK) return err;
        vTaskDelay(pdMS_TO_TICKS(1000 / MAX30102_SAMPLE_RATE_HZ));
    }

    return ESP_OK;
}
