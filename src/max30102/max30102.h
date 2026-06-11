#ifndef MAX30102_PROCESSOR_H
#define MAX30102_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>

#define SPO2_BUFFER_SIZE 100

typedef struct
{
    uint32_t ir_buffer[SPO2_BUFFER_SIZE];
    uint32_t red_buffer[SPO2_BUFFER_SIZE];

    int32_t heart_rate;
    int32_t spo2;

    int8_t hr_valid;
    int8_t spo2_valid;

    uint8_t initialized;
} max30102_processor_t;

void max30102_processor_init(max30102_processor_t *ctx);

bool max30102_processor_add_sample(
    max30102_processor_t *ctx,
    uint32_t red,
    uint32_t ir
);

#endif