
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdio.h>

//#include "wifi.h"
#include "mqtt.h"
#include "mpu6050.h"
#include "max30102/max30102.h"
#include "max30102/max30102_driver.h"
#include "data_processing.h" 
    
/*
void publish_sensor_data(void *pvParameters)
{
  sensor_msg_t msg;

  while (1) {
      if (xQueueReceive(sensor_queue, &msg, portMAX_DELAY) == pdTRUE) {
          // Processa os dados e publica no MQTT
          if (msg.type == SENSOR_MPU6050) {
              // Processa dados do MPU6050
              mqtt_publish_message("health/device/mpu6050", &msg.mpu);
          } else if (msg.type == SENSOR_MAX30102) {
              // Processa dados do MAX30102
              mqtt_publish_message("health/device/max30102", &msg.max);
          }
      }
  }
}
  */



/* Função Principal */


void data_processing_task(void *pvParameters)
{
    // Esta task pode ser usada para processar os dados dos sensores e publicar no MQTT
    // Por enquanto, os sensores já estão lendo e publicando, então esta task pode ficar vazia
}


/* Recepção dos dados */

/* Analise */

/* publicação MQTT */