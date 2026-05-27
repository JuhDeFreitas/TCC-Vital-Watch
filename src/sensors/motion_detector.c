/**
 * Modulo de processamento de sinal do MPU6050 para detecção de movimento
 */

#include "sensors/motion_detector.h"
#include "sensors/mpu6050.h"

#include <math.h>

#include "esp_timer.h"
#include "esp_log.h"

/*  Variables =================================================================== */

//activity_mode_t g_user_status = USER_RESTING; 

static const char *TAG = "MOTION_DETECTOR";

static float previous_mag = 0.0f;

static int64_t last_peak_time = 0;

static int running_steps = 0;

static bool running_state = false;

/*  Public Functions =========================================================== */

bool detect_running(float ax, float ay, float az)
{
    float magnitude = sqrtf(
        (ax * ax) +
        (ay * ay) +
        (az * az)
    );

    magnitude = fabsf(magnitude - 1.0f);

    bool peak_detected = (
        magnitude > PEAK_THRESHOLD &&
        previous_mag <= PEAK_THRESHOLD
    );

    previous_mag = magnitude;

    if (!peak_detected){
        return running_state;
    }

    int64_t now = esp_timer_get_time() / 1000;

    int64_t delta = now - last_peak_time;

    last_peak_time = now;

    if ( delta > MIN_INTERVAL_MS && delta < MAX_INTERVAL_MS){
        running_steps++;

        ESP_LOGI(TAG, "STEP [%d]", running_steps);
    }
    else{
        running_steps = 0;
    }

    if (running_steps >= REQUIRED_STEPS){
        if (!running_state){
            ESP_LOGW(TAG, "RUNNING DETECTED");
        }

        running_state = true;
    }

    if (delta > 1000){
        running_state = false;
        running_steps = 0;
    }

    return running_state;
}

//void motion_task(void *arg)
//{
//    int16_t ax_raw;
//    int16_t ay_raw;
//    int16_t az_raw;
//
//    while (1)
//    {
//        /* Espera por notificação da tarefa */
//        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
//        
//        ESP_LOGI(TAG, "GPIO LEVEL: %d", gpio_get_level(MPU6050_INT_PIN));
//
//        /* Verifica o status do MPU6050 */
//        uint8_t status = mpu_status();
//
//        ESP_LOGI(TAG, "INT_STATUS = 0x%02X", status);
//
//        if (!(status & 0x40)) {
//            continue;
//        }
//
//        ESP_LOGI(TAG, "MOVEMENT INTERRUPT");
//
//        for (int i = 0; i < 100; i++)
//        {   
//            /* Realiza leitura do acelerometro */
//            mpu_read_accel(&ax_raw, &ay_raw, &az_raw);
//
//            float ax = ax_raw / 16384.0f;
//            float ay = ay_raw / 16384.0f;
//            float az = az_raw / 16384.0f;
//
//            /* Detecta movimento */
//            bool running = detect_running(ax, ay, az);
//
//            /* Atualiza o status de movimento */
//            if (running){
//                if( g_user_status != USER_RUNNING ){
//                    g_user_status  = USER_RUNNING;
//                    ESP_LOGW(TAG, "USER STARTED RUNNING");                    
//                }
//            }
//            else{
//                if( g_user_status != USER_RESTING ){
//                    g_user_status  = USER_RESTING;
//                    ESP_LOGW(TAG, "USER STOPPED RUNNING");
//                }
//            }
//
//            vTaskDelay(pdMS_TO_TICKS(20));
//        }
//    }
//}