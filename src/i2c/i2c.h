#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

/* Configurações do barramento I2C */
#define I2C_MASTER_SCL_IO          9
#define I2C_MASTER_SDA_IO          8
#define I2C_MASTER_NUM             I2C_NUM_0
#define I2C_MASTER_FREQ_HZ         400000
#define I2C_MASTER_TX_BUF_DISABLE  0
#define I2C_MASTER_RX_BUF_DISABLE  0

/* Endereço I2C do MAX30102 */
#define MAX30102_ADDR  0x57

/** Inicializa o mestre I2C */
esp_err_t i2c_init(void);
void      i2c_start(void);

/** Escreve bytes para um dispositivo no barramento */
esp_err_t i2c_write(uint8_t dev_addr, uint8_t *data, size_t size);

/** Lê bytes de um dispositivo no barramento */
esp_err_t i2c_read(uint8_t dev_addr, uint8_t *data, size_t size);

/** Escreve um registro e lê a resposta (repeated start) */
esp_err_t i2c_write_read(uint8_t dev_addr, uint8_t *wr_data, size_t wr_size,
                         uint8_t *rd_data, size_t rd_size);

#endif // I2C_H
