#include "sensors/max30102_driver.h"
#include "i2c.h"
#include "esp_log.h"
#include "freertos/task.h"


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

static const char *TAG = "MAX30102_DRIVER";

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    return i2c_master_write_to_device(I2C_MASTER_NUM, MAX30102_ADDR,
                                      data, 2, pdMS_TO_TICKS(1000));
}

static esp_err_t read_multi(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM,
                                        MAX30102_ADDR,
                                        &reg, 1,
                                        buf, len,
                                        pdMS_TO_TICKS(1000));
}

esp_err_t max30102_init(void)
{
    uint8_t id;
    read_multi(REG_PART_ID, &id, 1);
    ESP_LOGI(TAG, "ID=0x%02X", id);

    write_reg(REG_MODE_CONFIG, 0x40);
    vTaskDelay(pdMS_TO_TICKS(100));

    write_reg(REG_SPO2_CONFIG, 0x27);
    write_reg(REG_LED1_PA, 0x24);
    write_reg(REG_LED2_PA, 0x24);
    write_reg(REG_MODE_CONFIG, 0x03);

    return ESP_OK;
}

esp_err_t max30102_read_sample(uint32_t *red, uint32_t *ir)
{
    uint8_t raw[6];
    if (read_multi(REG_FIFO_DATA, raw, 6) != ESP_OK) return ESP_FAIL;

    *red = ((raw[0]<<16)|(raw[1]<<8)|raw[2]) & 0x3FFFF;
    *ir  = ((raw[3]<<16)|(raw[4]<<8)|raw[5]) & 0x3FFFF;

    return ESP_OK;
}

esp_err_t max30102_collect_window(uint32_t *red_buf,
                                  uint32_t *ir_buf,
                                  int len)
{
    for(int i=0;i<len;i++){
        if(max30102_read_sample(&red_buf[i], &ir_buf[i])!=ESP_OK)
            return ESP_FAIL;
        vTaskDelay(pdMS_TO_TICKS(1000/MAX30102_SAMPLE_RATE_HZ));
    }
    return ESP_OK;
}