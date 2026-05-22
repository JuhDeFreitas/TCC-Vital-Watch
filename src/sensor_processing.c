// process_sample()
// filter_sample()
// validate_sample()
// update_sensor_state()

#include "sensor_processing.h"

/* Private definitions ====================================================== */
//static const char *TAG = "SENSOR PROCESSING";
static const char *TAG = "ALERT_MANAGER";


/* Crição da task dos sensores */
TaskHandle_t mpu6050_handle = NULL;
TaskHandle_t max30102_handle = NULL;
TaskHandle_t alert_manager_handle = NULL;

/* Private functions ====================================================== */

/* Task for sensors processing ========================================== */


/* Public Functions  ====================================================== */

void sensor_processing_init(void)
{
    //mpu_queue = xQueueCreate(1, sizeof(mpu6050_data_t));
   // max_queue = xQueueCreate(1, sizeof(max30102_data_t));

    ///init_sensors();

    ESP_LOGI(TAG, "Inicialização dos sensores...");
    xTaskCreate(
        mpu6050_task,
        "MPU6050 Task",
        4096,
        NULL,
        5,
        &mpu6050_handle   
    );

    xTaskCreate(
        max30102_task,
        "MAX30102 Task",
        8192,
        NULL,
        5,
        &max30102_handle  
    );

    xTaskCreate(
        alert_manager_task,   // função da task
        "alert_manager",      // nome da task
        4096,                 // stack size
        NULL,                 // parâmetros
        5,                    // prioridade
        NULL                  // handle opcional
    );
}

