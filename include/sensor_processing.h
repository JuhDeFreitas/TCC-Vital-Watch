#ifndef SENSOR_PROCESSING_H
#define SENSOR_PROCESSING_H

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_err.h"

#include "sensors/mpu6050.h"
#include "sensors/max30102.h"
#include "alert_manager.h"

void sensor_processing_init(void);

#endif