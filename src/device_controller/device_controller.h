#ifndef DEVICE_CONTROLLER_H
#define DEVICE_CONTROLLER_H

typedef enum {
    DEVICE_START,
    DEVICE_STOP,
    DEVICE_REBOOT,
} device_state_t;

extern volatile device_state_t g_device_state;

// Transita para o novo estado e executa a ação correspondente.
// DEVICE_START  → resume as tasks dos sensores
// DEVICE_STOP   → suspende as tasks dos sensores
// DEVICE_REBOOT → reinicia a placa via esp_restart()
void device_set_state(device_state_t new_state);

#endif
