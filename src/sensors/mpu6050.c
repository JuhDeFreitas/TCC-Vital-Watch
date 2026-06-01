#include "sensors/mpu6050.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"

#include "esp_log.h"

static const char *TAG = "MPU6050";

TaskHandle_t mpu_motion_task_handle = NULL;

static activity_state_t mpu_state = USER_RESTING; 

/* Private Functions ================================================ */

static void write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};

    esp_err_t err = i2c_master_write_to_device(
        I2C_NUM_0,
        MPU6050_ADDR,
        data,
        sizeof(data),
        pdMS_TO_TICKS(100)
    );

    ESP_LOGI(TAG,
        "WRITE reg 0x%02X = 0x%02X -> %s",
        reg,
        value,
        esp_err_to_name(err)
    );
}

static void read_regs(uint8_t reg, uint8_t *buffer, size_t len)
{
    i2c_master_write_read_device(
        I2C_NUM_0,
        MPU6050_ADDR,
        &reg,
        1,
        buffer,
        len,
        pdMS_TO_TICKS(100)
    );
}

static void IRAM_ATTR mpu_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    vTaskNotifyGiveFromISR(
        mpu_motion_task_handle,
        &xHigherPriorityTaskWoken
    );

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* Public Functions ================================================ */

uint8_t mpu_reg_status(void)
{
    uint8_t status;

    read_regs(0x3A, &status, 1);

    return status;
}

activity_state_t mpu_get_activity_state(void)
{
    return mpu_state;
}

void mpu_init(void)
{
    /* wake up MPU */
    write_reg(0x6B, 0x00);

    /* accel +-2g */
    write_reg(0x1C, 0x00);

    ESP_LOGI(TAG, "MPU6050 initialized");
}

void mpu_config_motion_interrupt(void)
{
    /* Wake up MPU */
    write_reg(0x6B, 0x00);

    vTaskDelay(pdMS_TO_TICKS(100));

    /* Reset signal paths */
    write_reg(0x68, 0x07);

    /* DLPF: 20Hz para menos ruído */
    write_reg(0x1A, 0x04);

    /* Sample Rate Divider: 50Hz */
    write_reg(0x19, 0x13);

    /* Motion threshold: valor mínimo (1 LSB = 2mg, 0x01 = 2mg) */
    write_reg(0x1F, 0x01);

    /* Motion duration: valor mínimo (1 LSB = 1ms, 0x01 = 1ms) */
    write_reg(0x20, 0x01);

    /* Motion detect control: modo mais sensível (acel high-pass, latch, counter decrement = 0) */
    write_reg(0x69, 0x15);

    /* Interrupt pin config */
    write_reg(0x37, 0x20);

    /* Enable motion interrupt */
    write_reg(0x38, 0x40);

    ESP_LOGI(TAG, "Motion interrupt enabled (sensibilidade máxima)");
}

void mpu_read_accel(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t data[6];

    read_regs(0x3B, data, 6);

    *ax = (data[0] << 8) | data[1];
    *ay = (data[2] << 8) | data[3];
    *az = (data[4] << 8) | data[5];
}

void mpu_gpio_interrupt_init(void)
{
    gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << MPU6050_INT_PIN),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_ENABLE,
      .intr_type = GPIO_INTR_POSEDGE
  };

    gpio_config(&io_conf);

    gpio_install_isr_service(0);

    gpio_isr_handler_add(
        MPU6050_INT_PIN,
        mpu_isr_handler,
        NULL
    );

    ESP_LOGI(TAG, "GPIO interrupt initialized");
}

void mpu_motion_task(void *arg)
{
    int16_t ax_raw;
    int16_t ay_raw;
    int16_t az_raw;

    while (1)
    {
        /* Espera por notificação da tarefa */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        ESP_LOGI(TAG, "GPIO LEVEL: %d", gpio_get_level(MPU6050_INT_PIN));

        /* Verifica o status do MPU6050 */
        uint8_t status = mpu_reg_status();

        ESP_LOGI(TAG, "INT_STATUS = 0x%02X", status);

        if (!(status & 0x40)) {
            continue;
        }

        ESP_LOGI(TAG, "MOVEMENT INTERRUPT");

        for (int i = 0; i < 100; i++)
        {   
            /* Realiza leitura do acelerometro */
            mpu_read_accel(&ax_raw, &ay_raw, &az_raw);

            float ax = ax_raw / 16384.0f;
            float ay = ay_raw / 16384.0f;
            float az = az_raw / 16384.0f;

            /* Detecta movimento */
            bool running = detect_running(ax, ay, az);

            /* Atualiza o status de movimento */
            if (running){
                if( mpu_state != USER_RUNNING ){
                    mpu_state  = USER_RUNNING;
                    ESP_LOGW(TAG, "USER STARTED RUNNING");                    
                }
            }
            else{
                if( mpu_state != USER_RESTING ){
                    mpu_state  = USER_RESTING;
                    ESP_LOGW(TAG, "USER STOPPED RUNNING");
                }
            }

            vTaskDelay(pdMS_TO_TICKS(40));
        }
    }
}

