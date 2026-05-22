#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float heart_rate_bpm;
    float spo2_percent;
    bool hr_valid;
    bool spo2_valid;
} max30102_data_t;

extern max30102_data_t g_max_data; 

void max30102_task(void *pvParameters);