#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include <stdint.h>

typedef struct {

    uint32_t sampling_interval_ms;
    uint32_t publish_interval_ms;

} sampling_config_t;

extern sampling_config_t g_sampling_config;

/* =============== PUBLIC FUNCTIONS =============== */

/* Device ID */

void load_device_id(void);

const char *device_get_id(void);

/* Sampling Configuration */

void load_sampling_config(void);

void set_sampling_config(const char *data);


uint32_t get_sampling_interval(void);
uint32_t get_publish_interval(void);




#endif