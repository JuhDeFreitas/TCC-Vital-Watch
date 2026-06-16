#pragma once

#include <stdint.h>
#include "esp_err.h"

#define MAX30102_ADDR 0x57
#define MAX30102_SAMPLE_RATE_HZ 100
#define MAX30102_BUFFER_SIZE 100

esp_err_t max30102_init(void);
esp_err_t max30102_read_sample(uint32_t *red, uint32_t *ir);
esp_err_t max30102_collect_window(uint32_t *red_buf,
                                  uint32_t *ir_buf,
                                  int len);