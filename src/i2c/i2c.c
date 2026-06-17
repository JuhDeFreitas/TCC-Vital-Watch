#include "i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t i2c_init(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_MASTER_SDA_IO,
        .scl_io_num       = I2C_MASTER_SCL_IO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) return err;

    return i2c_driver_install(
        I2C_MASTER_NUM,
        conf.mode,
        I2C_MASTER_RX_BUF_DISABLE,
        I2C_MASTER_TX_BUF_DISABLE,
        0
    );
}

void i2c_start(void)
{
    ESP_ERROR_CHECK(i2c_init());
    ESP_LOGI("I2C", "I2C Master inicializado");
}

esp_err_t i2c_write(uint8_t dev_addr, uint8_t *data, size_t size)
{
    return i2c_master_write_to_device(I2C_MASTER_NUM, dev_addr, data, size,
                                      pdMS_TO_TICKS(1000));
}

esp_err_t i2c_read(uint8_t dev_addr, uint8_t *data, size_t size)
{
    return i2c_master_read_from_device(I2C_MASTER_NUM, dev_addr, data, size,
                                       pdMS_TO_TICKS(1000));
}

esp_err_t i2c_write_read(uint8_t dev_addr, uint8_t *wr_data, size_t wr_size,
                         uint8_t *rd_data, size_t rd_size)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, dev_addr,
                                        wr_data, wr_size,
                                        rd_data, rd_size,
                                        pdMS_TO_TICKS(1000));
}
