#include <stdio.h>

#include "wifi/wifi.h"
#include "mqtt/mqtt.h"

#include "i2c/i2c.h"
#include "mpu6050/mpu6050.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    wifi_init();
    wifi_start();
    start_i2c();

    mqtt_init();

    // Cria a task de leitura do MPU6050 em paralelo
    xTaskCreate(
        start_mpu6050,      // Função da task
        "MPU6050 Task",    // Nome da task
        4096,              // Stack size (ajuste conforme necessário)
        NULL,              // Parâmetro (se precisar)
        5,                 // Prioridade
        NULL               // Handle (opcional)
    );

    while (1) {
        mqtt_publish_message("Hello ESP32");
        vTaskDelay(pdMS_TO_TICKS(5000)); // espera 5s
        printf("Message published\n");
    }
}