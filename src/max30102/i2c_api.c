#include "i2c_api.h"

esp_err_t i2c_init(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = SDA_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_io_num       = SCL_PIN,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_CLK_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_PORT, &conf);
    if (err != ESP_OK) return err;
    return i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
}

esp_err_t i2c_sensor_write(uint8_t *data_wr, size_t size)
{
    return i2c_master_write_to_device(I2C_PORT, MAX30102_ADDR, data_wr, size,
                                      pdMS_TO_TICKS(1000));
}

esp_err_t i2c_sensor_read(uint8_t *data_rd, size_t size)
{
    return i2c_master_read_from_device(I2C_PORT, MAX30102_ADDR, data_rd, size,
                                       pdMS_TO_TICKS(1000));
}
