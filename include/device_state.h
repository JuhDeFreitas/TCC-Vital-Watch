#ifndef DEVICE_STATE_H
#define DEVICE_STATE_H

typedef enum {
    DEVICE_STOP = 0,
    DEVICE_START,
    DEVICE_REBOOT
} device_state_t;

void device_state_init(void);

void set_device_state(device_state_t new_state);

device_state_t get_device_state(void);

#endif