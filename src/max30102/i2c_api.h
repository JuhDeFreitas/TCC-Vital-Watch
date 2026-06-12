#ifndef I2C_API_H
#define I2C_API_H

#include "esp_err.h"
#include "driver/i2c.h"

#define I2C_PORT           I2C_NUM_0
#define MAX30102_ADDR      0x57
#define SDA_PIN            8
#define SCL_PIN            9
#define I2C_CLK_FREQ_HZ    100000

esp_err_t i2c_init(void);
esp_err_t i2c_sensor_write(uint8_t *data_wr, size_t size);
esp_err_t i2c_sensor_read(uint8_t *data_rd, size_t size);

#endif
