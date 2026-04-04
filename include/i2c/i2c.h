
#ifndef I2C_H
#define I2C_H

#include "driver/i2c.h"

esp_err_t i2c_master_init(void);
void start_i2c(void);

#endif // I2C_H