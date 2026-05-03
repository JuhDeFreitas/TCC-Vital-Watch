#include "device_state.h"

device_state_t device_state = DEVICE_STOP;

void set_device_state(device_state_t new_state)
{
    device_state = new_state;
}