#ifndef DATA_PROCESSING_H
#define DATA_PROCESSING_H 

#include "mpu6050.h"
#include "max30102/max30102.h"

typedef enum {
    SENSOR_MPU6050,
    SENSOR_MAX30102
} sensor_type_t;

typedef struct {
    sensor_type_t type;

    union {
        mpu6050_data_t mpu;
        max30102_data_t max;
    };

} sensor_msg_t;



#endif // DATA_PROCESSING_H