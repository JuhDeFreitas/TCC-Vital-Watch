#include "device_controller.h"

#include "mpu6050.h"
#include "max30102_api.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "DEVICE";

volatile device_state_t g_device_state = DEVICE_STOP;

static void device_start(void)
{
    ESP_LOGI(TAG, "START — sensores ativos");
    mpu6050_resume();
    max30102_resume();
    g_device_state = DEVICE_START;
}

static void device_stop(void)
{
    ESP_LOGI(TAG, "STOP — sensores pausados");
    mpu6050_suspend();
    max30102_suspend();
    g_device_state = DEVICE_STOP;
}

static void device_reboot(void)
{
    ESP_LOGW(TAG, "REBOOT — reiniciando...");
    esp_restart();
}

void device_set_state(device_state_t new_state)
{
    if (new_state == g_device_state && new_state != DEVICE_REBOOT) return;

    switch (new_state) {
    case DEVICE_START:  device_start();  break;
    case DEVICE_STOP:   device_stop();   break;
    case DEVICE_REBOOT: device_reboot(); break;
    }
}
