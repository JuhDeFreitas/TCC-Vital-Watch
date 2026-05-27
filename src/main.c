#include <stdio.h>

#include "driver/i2c.h"
#include "wifi.h"
#include "mqtt/mqtt.h"
#include "i2c.h"
#include "sensors/mpu6050.h"
#include "sensors/motion_detector.h"
#include "sensors/max30102.h"
#include "sensors/max30102_driver.h"
#include "alert_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "device_state.h"

#include "esp_log.h"

// Definições dos pinos I2C
#define I2C_SDA 8
#define I2C_SCL 9

// Declarações externas para tasks e handles do MPU6050
extern void mpu_motion_task(void *pvParameters);

extern TaskHandle_t mpu_motion_task_handle;



static const char *TAG = "MAIN";

#define PATIENT_ID "patient_001"

// Mutex global do barramento I2C
SemaphoreHandle_t i2c_mutex = NULL;

//TaskHandle_t mpu6050_handle = NULL;
TaskHandle_t max30102_handle = NULL;


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
    while(!wifi_is_connected()) {
        //ESP_LOGE(TAG, "Falha ao iniciar Wi-Fi.");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    device_state_init();

    // Termina a inicialização do sistema quando o Wi-Fi estiver conectado
    start_i2c();
    mqtt_init();

    xTaskCreate(
        max30102_task,
        "MAX30102 Task",
        8192,
        NULL,
        5,
        &max30102_handle  
    );

    /*inicializa o sensor de movimento*/
    mpu_init();

}


/* Funções principais */
void app_main(void)
{
    ESP_LOGI(TAG, "     ");
    ESP_LOGI(TAG, "     ");
    ESP_LOGI(TAG, "_______________________________");
    ESP_LOGI(TAG, "_________ Vital Watch _________");

    /* Inicializa os sensores */
    i2c_master_init();
    mpu_init();
    
    /* Configura GPIO + ISR  */
    mpu_gpio_interrupt_init();

    /* Inicializa a task do sensor de movimento */
    xTaskCreate(
        mpu_motion_task,
        "MP6050 Motion Task",
        4096,
        NULL,
        5,
        &mpu_motion_task_handle
    );

    vTaskDelay(pdMS_TO_TICKS(100));

    /* Habilita/Configura o interrupt de movimento do MPU6050 */
    mpu_config_motion_interrupt();

    ESP_LOGI("MAIN", "System started");

    /* Inicializa a task do gerenciador de alertas */
    xTaskCreate(
        alert_manager_task,
        "alert_manager_task",
        4096,
        NULL,
        5,
        NULL
    );

    /* LOOP */
    while (1){
        ESP_LOGI(TAG, "USER STATUS: %d" , mpu_get_activity_state());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    //system_init();

    set_device_state(DEVICE_START);

    ESP_LOGI(TAG, "Sistema pronto. Aguardando comando MQTT...");
}

