#include "mpu6050.h"
#include "i2c.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "data_processing.h"
#include "mqtt.h"

static const char *TAG = "MPU6050";

/* Driver MPU6050 */

esp_err_t mpu6050_write_byte(uint8_t reg, uint8_t data)
{
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_write_to_device(
        I2C_MASTER_NUM,
        MPU6050_ADDR,
        write_buf,
        sizeof(write_buf),
        pdMS_TO_TICKS(1000)
    );
}

esp_err_t mpu6050_read_bytes(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(
        I2C_MASTER_NUM,
        MPU6050_ADDR,
        &reg,
        1,
        data,
        len,
        pdMS_TO_TICKS(1000)
    );
}


/* Logica de leitura do sensor */

static esp_err_t wakeup_mpu6050(void)
{
    /* 
    * Tira o MPU6050 do sleep 
    */
    esp_err_t ret = mpu6050_write_byte(MPU6050_REG_PWR_MGMT_1, 0x00);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao acordar MPU6050: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "MPU6050 acordado");

    return ESP_OK;
}


static esp_err_t mpu6050_check_identity(void)
{
/* 
 *  Verifica se o MPU6050 está respondendo corretamente via I2C
 */
    uint8_t who_am_i = 0;

    esp_err_t ret = mpu6050_read_bytes(
        MPU6050_REG_WHO_AM_I,
        &who_am_i,
        1
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro I2C ao ler WHO_AM_I: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    if (who_am_i != MPU6050_WHO_AM_I_EXPECTED) {
        ESP_LOGE(TAG,
                 "Dispositivo inesperado! WHO_AM_I=0x%02X (esperado=0x%02X)",
                 who_am_i,
                MPU6050_WHO_AM_I_EXPECTED);
        return ESP_ERR_INVALID_RESPONSE;
    }

    ESP_LOGI(TAG, "MPU6050 detectado (WHO_AM_I=0x%02X)", who_am_i);

    return ESP_OK;
}


static esp_err_t mpu6050_read_data(mpu6050_data_t *out)
{ 
    /*
    * Realiza a leitura dos dados do MPU6050 e retorna uma estrutura com os valores lidos
    */
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw[14];

    esp_err_t err = mpu6050_read_bytes(
        MPU6050_REG_ACCEL_XOUT_H,
        raw,
        14
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao ler MPU6050: %s", esp_err_to_name(err));
        return err;
    }

    /* ================= ACCEL ================= */
    out->accel_x = (int16_t)((raw[0] << 8) | raw[1]);
    out->accel_y = (int16_t)((raw[2] << 8) | raw[3]);
    out->accel_z = (int16_t)((raw[4] << 8) | raw[5]);

    /* ================= TEMP ================= */
    int16_t temp_raw = (int16_t)((raw[6] << 8) | raw[7]);
    out->temperature = (temp_raw / 340.0f) + 36.53f;

    /* ================= GYRO ================= */
    out->gyro_x = (int16_t)((raw[8] << 8) | raw[9]);
    out->gyro_y = (int16_t)((raw[10] << 8) | raw[11]);
    out->gyro_z = (int16_t)((raw[12] << 8) | raw[13]);

    return ESP_OK;
}

/* Task */
void mpu6050_task(void *pvParameters)
{
    if (wakeup_mpu6050() != ESP_OK) {
        vTaskDelete(NULL);
    }

    if (mpu6050_check_identity() != ESP_OK) {
        vTaskDelete(NULL);
    }

    while (1) {
        mpu6050_data_t data;

        if (mpu6050_read_data(&data) == ESP_OK) {

            ESP_LOGI(TAG,
                "ACC[%d %d %d] GYRO[%d %d %d] TEMP=%.2f",
                data.accel_x,
                data.accel_y,
                data.accel_z,
                data.gyro_x,
                data.gyro_y,
                data.gyro_z,
                data.temperature
            );

            /* Publicação no Broker MQTT */
            mqtt_publish_message(TOPIC_HR, &data);

        } else {
            ESP_LOGW(TAG, "Erro na leitura do MPU6050");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}