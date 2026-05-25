#include "sensors/mpu6050_driver.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"

#include "esp_log.h"

static const char *TAG = "MPU6050";

TaskHandle_t motion_task_handle = NULL;

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

static void read_regs(
    uint8_t reg,
    uint8_t *buffer,
    size_t len
)
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

uint8_t read_int_status(void)
{
    uint8_t status;

    read_regs(0x3A, &status, 1);

    return status;
}

void mpu6050_init(void)
{
    /* wake up MPU */
    write_reg(0x6B, 0x00);

    /* accel +-2g */
    write_reg(0x1C, 0x00);

    ESP_LOGI(TAG, "MPU6050 initialized");
}

void mpu6050_enable_motion_interrupt(void)
{
    /* Wake up MPU */
    write_reg(0x6B, 0x00);

    vTaskDelay(pdMS_TO_TICKS(100));

    /* Reset signal paths */
    write_reg(0x68, 0x07);

    /* DLPF */
    write_reg(0x1A, 0x00);

    /* Motion threshold */
    write_reg(0x1F, 0x04);

    /* Motion duration */
    write_reg(0x20, 0x04);

    /* Motion detect control */
    write_reg(0x69, 0x15);

    /* Interrupt pin config */
    write_reg(0x37, 0x20);

    /* Enable motion interrupt */
    write_reg(0x38, 0x40);

    ESP_LOGI(TAG, "Motion interrupt enabled");
}

void mpu6050_read_accel_raw(
    int16_t *ax,
    int16_t *ay,
    int16_t *az
)
{
    uint8_t data[6];

    read_regs(0x3B, data, 6);

    *ax = (data[0] << 8) | data[1];
    *ay = (data[2] << 8) | data[3];
    *az = (data[4] << 8) | data[5];
}

static void IRAM_ATTR mpu_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    vTaskNotifyGiveFromISR(
        motion_task_handle,
        &xHigherPriorityTaskWoken
    );

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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