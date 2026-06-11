
#include "spo2_algorithm.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "max30102.h"

void max30102_processor_init(max30102_processor_t *ctx)
{
    memset(ctx, 0, sizeof(max30102_processor_t));
}

bool max30102_processor_add_sample(
    max30102_processor_t *ctx,
    uint32_t red,
    uint32_t ir)
{
    static uint16_t sample_count = 0;

    if(sample_count < 100)
    {
        ctx->red_buffer[sample_count] = red;
        ctx->ir_buffer[sample_count]  = ir;

        sample_count++;

        if(sample_count < 100)
            return false;

        maxim_heart_rate_and_oxygen_saturation(
            ctx->ir_buffer,
            100,
            ctx->red_buffer,
            &ctx->spo2,
            &ctx->spo2_valid,
            &ctx->heart_rate,
            &ctx->hr_valid
        );

        ctx->initialized = 1;

        return true;
    }

    memmove(
        ctx->red_buffer,
        &ctx->red_buffer[25],
        75 * sizeof(uint32_t)
    );

    memmove(
        ctx->ir_buffer,
        &ctx->ir_buffer[25],
        75 * sizeof(uint32_t)
    );

    ctx->red_buffer[99] = red;
    ctx->ir_buffer[99] = ir;

    static uint8_t update_counter = 0;

    update_counter++;

    if(update_counter >= 25)
    {
        update_counter = 0;

        maxim_heart_rate_and_oxygen_saturation(
            ctx->ir_buffer,
            100,
            ctx->red_buffer,
            &ctx->spo2,
            &ctx->spo2_valid,
            &ctx->heart_rate,
            &ctx->hr_valid
        );

        return true;
    }

    return false;
}