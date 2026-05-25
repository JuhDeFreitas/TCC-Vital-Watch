#ifndef MOTION_DETECTOR_H
#define MOTION_DETECTOR_H

#include <stdbool.h>

typedef enum
{
    PATIENT_STATE_RESTING = 0,
    PATIENT_STATE_RUNNING
} patient_state_t;

bool detect_running(float ax, float ay, float az);

void motion_task(void *arg);

#endif