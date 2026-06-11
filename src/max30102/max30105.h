#ifndef MAX30105_H
#define MAX30105_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"   // ou Wire abstraction equivalente

#ifdef __cplusplus
extern "C" {
#endif

#define MAX30105_ADDRESS 0x57

// FIFO / config sizes
#define STORAGE_SIZE 32
#define I2C_BUFFER_LENGTH 32

typedef struct {
    uint32_t red[STORAGE_SIZE];
    uint32_t IR[STORAGE_SIZE];
    uint32_t green[STORAGE_SIZE];
    uint8_t head;
    uint8_t tail;
} max30105_fifo_t;

typedef struct {
    i2c_port_t port;
    uint8_t addr;
    uint8_t activeLEDs;
    max30105_fifo_t sense;
} max30105_t;

// API pública
bool max30105_init(max30105_t *dev, i2c_port_t port, uint8_t addr);

void max30105_soft_reset(max30105_t *dev);
void max30105_setup(max30105_t *dev, uint8_t powerLevel, uint8_t sampleAverage,
                    uint8_t ledMode, int sampleRate, int pulseWidth, int adcRange);

uint8_t max30105_available(max30105_t *dev);
uint16_t max30105_check(max30105_t *dev);

uint32_t max30105_get_red(max30105_t *dev);
uint32_t max30105_get_ir(max30105_t *dev);
uint32_t max30105_get_green(max30105_t *dev);

void max30105_next_sample(max30105_t *dev);

#ifdef __cplusplus
}
#endif

#endif