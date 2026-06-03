#ifndef DEVICE_CONTROLLER_H
#define DEVICE_CONTROLLER_H

/**
 * @brief Device operating states.
 */
typedef enum
{
    DEVICE_STOP = 0,
    DEVICE_START,
    DEVICE_REBOOT
} device_state_t;

/**
 * @brief Update the current device state.
 *
 * @param new_state New device state to be applied.
 */
void set_device_state(device_state_t new_state);

/**
 * @brief Get the current device state.
 *
 * @return Current device state.
 */
device_state_t get_device_state(void);

#endif /* DEVICE_CONTROLLER_H */