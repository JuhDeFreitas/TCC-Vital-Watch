#ifndef MOTION_DETECTOR_H
#define MOTION_DETECTOR_H

#include <stdbool.h>

#define PEAK_THRESHOLD       0.35f

#define MIN_INTERVAL_MS      180
#define MAX_INTERVAL_MS      500

#define REQUIRED_STEPS       5

/*  Public Functions =========================================================== */

bool detect_running(float ax, float ay, float az);

//void motion_task(void *arg);

#endif