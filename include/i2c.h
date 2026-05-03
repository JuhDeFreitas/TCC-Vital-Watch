
#ifndef I2C_H
#define I2C_H

#include "driver/i2c.h"

/* Configurações do I2C */
#define I2C_MASTER_SCL_IO          9  // Pino do SCL
#define I2C_MASTER_SDA_IO          8  // Pino do SDA
#define I2C_MASTER_NUM             I2C_NUM_0  // Usando o I2C0
#define I2C_MASTER_FREQ_HZ         100000 // Frequência de 100kHz
#define I2C_MASTER_TX_BUF_DISABLE  0  
#define I2C_MASTER_RX_BUF_DISABLE  0  

/** \brief Inicializa o mestre I2C */
esp_err_t i2c_master_init(void);
void start_i2c(void);

#endif // I2C_H