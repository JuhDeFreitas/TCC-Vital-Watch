#include <stdio.h>

#include "driver/i2c.h"
#include "wifi.h"
#include "mqtt/mqtt.h"
#include "i2c.h"
#include "sensors/mpu6050.h"
#include "sensors/mpu6050_driver.h"
#include "sensors/motion_detector.h"
#include "sensors/max30102.h"
#include "sensors/max30102_driver.h"
#include "sensor_processing.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "device_state.h"

#include "esp_log.h"

// Definições dos pinos I2C
#define I2C_SDA 8
#define I2C_SCL 9

// Declarações externas para tasks e handles do MPU6050
extern void motion_task(void *pvParameters);
extern TaskHandle_t motion_task_handle;

static const char *TAG = "MAIN";

#define PATIENT_ID "patient_001"

// Mutex global do barramento I2C
SemaphoreHandle_t i2c_mutex = NULL;

//TaskHandle_t mpu6050_handle = NULL;
//TaskHandle_t max30102_handle = NULL;


/* Funções auxiliares */
/*
void init_sensors(void)
{
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


}*/


static void system_init(void)
{
    ESP_LOGI(TAG, "Configurando sistema...");

    wifi_init();
    wifi_start();
    while(!wifi_is_connected()) {
        //ESP_LOGE(TAG, "Falha ao iniciar Wi-Fi.");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    device_state_init();

    // Termina a inicialização do sistema quando o Wi-Fi estiver conectado
    start_i2c();
    mqtt_init();

    sensor_processing_init();

    // cria task de sensores
    //init_sensors();

    // garante que começam parados
    //if (mpu6050_handle) vTaskSuspend(mpu6050_handle);
    //if (max30102_handle) vTaskSuspend(max30102_handle);
}


/* Funções principais */
void app_main(void)
{
    ESP_LOGI(TAG, "     ");
    ESP_LOGI(TAG, "     ");
    ESP_LOGI(TAG, "_______________________________");
    ESP_LOGI(TAG, "_________ Vital Watch _________");

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000
    };

    i2c_param_config(I2C_NUM_0, &conf);

    i2c_driver_install(
        I2C_NUM_0,
        conf.mode,
        0,
        0,
        0
    );  

    mpu6050_init();

    xTaskCreate(
        motion_task,
        "motion_task",
        4096,
        NULL,
        5,
        &motion_task_handle
    );
    
    mpu_gpio_interrupt_init();

    vTaskDelay(pdMS_TO_TICKS(100));

    mpu6050_enable_motion_interrupt();

    ESP_LOGI("MAIN", "System started");

    //system_init();

    set_device_state(DEVICE_START);

    ESP_LOGI(TAG, "Sistema pronto. Aguardando comando MQTT...");
}

