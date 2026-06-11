#include "max30102_api.h"
#include "i2c_api.h"

void max30102_init(max_config *configuration)
{
    write_max30102_reg(configuration->data1,  REG_INTR_ENABLE_1);
    write_max30102_reg(configuration->data2,  REG_INTR_ENABLE_2);
    write_max30102_reg(configuration->data3,  REG_FIFO_WR_PTR);
    write_max30102_reg(configuration->data4,  REG_OVF_COUNTER);
    write_max30102_reg(configuration->data5,  REG_FIFO_RD_PTR);
    write_max30102_reg(configuration->data6,  REG_FIFO_CONFIG);
    write_max30102_reg(configuration->data7,  REG_MODE_CONFIG);
    write_max30102_reg(configuration->data8,  REG_SPO2_CONFIG);
    write_max30102_reg(configuration->data9,  REG_LED1_PA);
    write_max30102_reg(configuration->data10, REG_LED2_PA);
    write_max30102_reg(configuration->data11, REG_PILOT_PA);
    write_max30102_reg(configuration->data12, REG_MULTI_LED_CTRL1);
    write_max30102_reg(configuration->data13, REG_MULTI_LED_CTRL2);
}

void read_max30102_fifo(int32_t *red_data, int32_t *ir_data)
{
    uint8_t buf[6] = {0};
    uint8_t fifo_reg = REG_FIFO_DATA;

    // Transação única com repeated start: write(reg) → Sr → read(6 bytes)
    // Necessário para garantir leitura contínua do FIFO sem perder ponteiro
    i2c_master_write_read_device(I2C_PORT, MAX30102_ADDR,
                                 &fifo_reg, 1,
                                 buf, 6,
                                 pdMS_TO_TICKS(1000));

    // Cada canal ocupa 3 bytes (18 bits úteis, MSB primeiro)
    *red_data = ((int32_t)buf[0] << 16) | ((int32_t)buf[1] << 8) | buf[2];
    *ir_data  = ((int32_t)buf[3] << 16) | ((int32_t)buf[4] << 8) | buf[5];

    // Máscara de 18 bits (ADC do MAX30102)
    *red_data &= 0x3FFFF;
    *ir_data  &= 0x3FFFF;
}

void read_max30102_reg(uint8_t reg_addr, uint8_t *data_reg, size_t bytes_to_read)
{
    i2c_master_write_read_device(I2C_PORT, MAX30102_ADDR,
                                 &reg_addr, 1,
                                 data_reg, bytes_to_read,
                                 pdMS_TO_TICKS(1000));
}

void write_max30102_reg(uint8_t command, uint8_t reg)
{
    uint8_t data[2] = {reg, command};
    i2c_sensor_write(data, 2);
}

float get_max30102_temp(void)
{
    uint8_t int_part  = 0;
    uint8_t frac_part = 0;
    write_max30102_reg(1, REG_TEMP_CONFIG);
    read_max30102_reg(REG_TEMP_INTR, &int_part,  1);
    read_max30102_reg(REG_TEMP_FRAC, &frac_part, 1);
    // Fração em incrementos de 0.0625 °C (datasheet MAX30102)
    return (float)int_part + ((float)frac_part * 0.0625f);
}
