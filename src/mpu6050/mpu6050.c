#include "mpu6050.h"
#include "i2c.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "MPU6050";

// Limites ao quadrado — evita sqrtf() no loop de 50 Hz
#define HIGH_SQ ((int64_t)STEP_HIGH_G_LSB * STEP_HIGH_G_LSB)
#define LOW_SQ  ((int64_t)STEP_LOW_G_LSB  * STEP_LOW_G_LSB)

static TaskHandle_t        s_task_handle = NULL;
static mpu6050_motion_cb_t s_step_cb     = NULL;

// ─── Helpers I2C ─────────────────────────────────────────────────────────────

static esp_err_t mpu_write(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_write(MPU6050_ADDR, buf, sizeof(buf));
}

static esp_err_t mpu_read(uint8_t reg, uint8_t *out, size_t len)
{
    return i2c_write_read(MPU6050_ADDR, &reg, 1, out, len);
}

// ─── ISR — contexto de interrupção ───────────────────────────────────────────
// Mínima por design: apenas acorda a task. Nenhuma operação bloqueante aqui.

static void IRAM_ATTR mpu6050_isr_handler(void *arg)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_task_handle, &woken);
    portYIELD_FROM_ISR(woken);
}

// ─── Task de detecção ────────────────────────────────────────────────────────
//
// Algoritmo: peak detection com histerese e debounce
//
//  Magnitude² do vetor aceleração:
//    em repouso ≈ 16384² = 268 M  (1g)
//    corrida    ≈ 20000²–28000² = 400M–784M  (pico de impacto de cada passo)
//
//  Máquina de estados:
//    IDLE → ao cruzar HIGH_SQ: registra passo, entra em ABOVE
//    ABOVE → ao cair abaixo de LOW_SQ: volta para IDLE (pronto pro próximo)
//
//  Debounce: garante que passos com menos de STEP_DEBOUNCE_MS entre si
//  sejam ignorados (evita duplo disparo no mesmo impacto).

static void mpu6050_step_task(void *arg)
{
    uint8_t  raw[6];
    int16_t  ax, ay, az;
    bool     above          = false;
    uint32_t last_step_ms   = 0;
    uint32_t prev_step_ms   = 0;

    while (1) {
        // Bloqueia até a ISR sinalizar (DATA_RDY a 50 Hz → acorda a cada ~20 ms)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Lê os 6 bytes de aceleração bruta: [XH XL YH YL ZH ZL]
        if (mpu_read(MPU_REG_ACCEL_XOUT_H, raw, 6) != ESP_OK) continue;

        ax = (int16_t)((raw[0] << 8) | raw[1]);
        ay = (int16_t)((raw[2] << 8) | raw[3]);
        az = (int16_t)((raw[4] << 8) | raw[5]);

        // Magnitude² — orientação-independente, sem sqrtf()
        int64_t mag_sq = (int64_t)ax*ax + (int64_t)ay*ay + (int64_t)az*az;

        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        if (!above && mag_sq > HIGH_SQ) {
            // Borda de subida: magnitude cruzou o limiar de impacto
            above = true;

            if ((now_ms - last_step_ms) >= STEP_DEBOUNCE_MS) {
                // Cadência instantânea a partir do intervalo entre passos
                uint32_t cadence = 0;
                if (prev_step_ms > 0 && last_step_ms > prev_step_ms) {
                    uint32_t interval_ms = last_step_ms - prev_step_ms;
                    if (interval_ms > 0)
                        cadence = 60000u / interval_ms;  // passos/min
                }

                ESP_LOGD(TAG, "Passo: mag²=%lld  cadencia=%lu spm",
                         (long long)mag_sq, (unsigned long)cadence);

                prev_step_ms  = last_step_ms;
                last_step_ms  = now_ms;

                if (s_step_cb) s_step_cb(cadence);
            }

        } else if (above && mag_sq < LOW_SQ) {
            // Borda de descida: magnitude voltou abaixo da histerese
            above = false;
        }

        // Limpa o latch do pino INT lendo INT_STATUS
        uint8_t status;
        mpu_read(MPU_REG_INT_STATUS, &status, 1);
    }
}

// ─── Configuração dos registradores ──────────────────────────────────────────

static esp_err_t mpu6050_configure(void)
{
    esp_err_t err;

    // Acorda o sensor; usa oscilador interno de 8 MHz
    err = mpu_write(MPU_REG_PWR_MGMT_1, 0x00);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(10));

    // Taxa de amostragem: 1 kHz / (19 + 1) = 50 Hz
    err = mpu_write(MPU_REG_SMPLRT_DIV, 0x13);
    if (err != ESP_OK) return err;

    // DLPF = 2: banda passante 98 Hz para acelerômetro
    // Filtra ruído elétrico (>100 Hz) e mantém os transientes de passo (~1–10 Hz)
    err = mpu_write(MPU_REG_CONFIG, 0x02);
    if (err != ESP_OK) return err;

    // Fundo de escala ±2 g: máxima sensibilidade (16384 LSB/g)
    // Suficiente para corrida (picos típicos < 2.5 g)
    err = mpu_write(MPU_REG_ACCEL_CONFIG, 0x00);
    if (err != ESP_OK) return err;

    // INT ativo alto, push-pull, nível mantido até INT_STATUS ser lido
    err = mpu_write(MPU_REG_INT_PIN_CFG, 0x20);
    if (err != ESP_OK) return err;

    // Habilita DATA_RDY interrupt (bit 0): dispara a cada nova amostra (50 Hz)
    err = mpu_write(MPU_REG_INT_ENABLE, 0x01);
    return err;
}

// ─── Configuração da GPIO de interrupção ─────────────────────────────────────

static esp_err_t mpu6050_gpio_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << MPU6050_INT_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_POSEDGE,
    };

    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) return err;

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    return gpio_isr_handler_add(MPU6050_INT_GPIO, mpu6050_isr_handler, NULL);
}

// ─── API pública ─────────────────────────────────────────────────────────────

uint8_t mpu6050_who_am_i(void)
{
    uint8_t val = 0;
    mpu_read(MPU_REG_WHO_AM_I, &val, 1);
    return val;     // 0x68 = OK
}

esp_err_t mpu6050_init(mpu6050_motion_cb_t on_step)
{
    s_step_cb = on_step;

    uint8_t who = mpu6050_who_am_i();
    if (who != 0x68) {
        ESP_LOGE(TAG, "WHO_AM_I=0x%02X — sensor ausente ou AD0 errado", who);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "MPU6050 ok (WHO_AM_I=0x%02X)", who);

    esp_err_t err = mpu6050_configure();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro nos registradores: %s", esp_err_to_name(err));
        return err;
    }

    xTaskCreate(mpu6050_step_task, "mpu6050_step", 2048, NULL, 5, &s_task_handle);

    err = mpu6050_gpio_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro na GPIO %d: %s", MPU6050_INT_GPIO, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Deteccao de corrida ativa: high=%.2fg  low=%.2fg  debounce=%dms",
             STEP_HIGH_G_LSB / 16384.0f,
             STEP_LOW_G_LSB  / 16384.0f,
             STEP_DEBOUNCE_MS);
    return ESP_OK;
}
