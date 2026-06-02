#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MAX_TASK_INTERVAL_MS 1000

typedef struct {
    float heart_rate_bpm;
    float spo2_percent;
    bool hr_valid;
    bool spo2_valid;
} max30102_data_t;

extern max30102_data_t g_max_data; 

void max30102_task(void *pvParameters);

void max30102_task_suspend(void);
void max30102_task_resume(void);
