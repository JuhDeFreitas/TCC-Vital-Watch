#include "mpu6050.h"
#include "i2c.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MPU6050";

#define HIGH_SQ ((int64_t)STEP_HIGH_G_LSB * STEP_HIGH_G_LSB)
#define LOW_SQ  ((int64_t)STEP_LOW_G_LSB  * STEP_LOW_G_LSB)

static TaskHandle_t        s_task_handle = NULL;
static mpu6050_motion_cb_t s_motion_cb   = NULL;

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

// ─── ISR ─────────────────────────────────────────────────────────────────────

static void IRAM_ATTR mpu6050_isr_handler(void *arg)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_task_handle, &woken);
    portYIELD_FROM_ISR(woken);
}

// ─── Task de detecção de corrida ─────────────────────────────────────────────
//
// Sensor montado no pulso — movimento principal do balanço do braço no eixo X.
//
// Algoritmo — três critérios simultâneos:
//
//  1. Impacto dinâmico em X: remove a componente estática de gravidade via EMA
//     (filtro passa-alta, τ ≈ 320 ms) e exige que a amplitude residual supere
//     STEP_HIGH_G_LSB. Isso isola o balanço do braço e descarta movimento lento
//     ou inclinação estática do pulso.
//
//  2. Cadência: intervalo entre picos dentro de [RUN_MIN_INTERVAL_MS,
//     RUN_MAX_INTERVAL_MS]. Descarta movimentos fora do ritmo de corrida.
//
//  3. Ritmo consistente: intervalo atual dentro de ±RUN_RHYTHM_TOL_PCT% da
//     média dos últimos picos válidos. Descarta sacudidas isoladas.
//
//  Confirmação: RUN_CONFIRM_STEPS picos consecutivos válidos → MPU_EVENT_RUNNING.
//  Saída: sem pico válido por RUN_TIMEOUT_MS → MPU_EVENT_STOPPED.

typedef enum { STATE_IDLE, STATE_CONFIRMING, STATE_RUNNING } run_state_t;

static void mpu6050_step_task(void *arg)
{
    uint8_t    raw[6];
    int16_t    ax;
    bool       above         = false;
    uint32_t   last_peak_ms  = 0;
    uint32_t   last_valid_ms = 0;
    run_state_t state        = STATE_IDLE;

    // EMA de ax para estimar a componente estática de gravidade no eixo X.
    // Armazenado em ponto fixo: ax_lp = valor_real * 16  (4 bits fracionários).
    // alpha = 1/16  →  τ ≈ 16 amostras / 50 Hz = 320 ms.
    int32_t ax_lp       = 0;
    bool    ax_lp_ready = false;

    uint32_t ring[RUN_CONFIRM_STEPS];
    uint8_t  ring_head  = 0;
    uint8_t  ring_count = 0;
    uint32_t ring_sum   = 0;
    memset(ring, 0, sizeof(ring));

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Limpa o latch do pino INT antes de qualquer continue
        uint8_t int_status;
        mpu_read(MPU_REG_INT_STATUS, &int_status, 1);

        if (mpu_read(MPU_REG_ACCEL_XOUT_H, raw, 6) != ESP_OK) continue;

        ax = (int16_t)((raw[0] << 8) | raw[1]);
        // raw[2..5] (ay, az) lidos mas não usados — detecção só no eixo X (pulso)

        // Inicializa o EMA na primeira leitura para evitar falsos positivos
        // durante o aquecimento do filtro.
        if (!ax_lp_ready) {
            ax_lp       = (int32_t)ax << 4;
            ax_lp_ready = true;
        }

        // Atualiza EMA: ax_lp = ax_lp * (15/16) + ax * (1/16)
        ax_lp += (int32_t)ax - (ax_lp >> 4);

        // Componente dinâmica: remove a gravidade estimada
        int16_t ax_dyn  = ax - (int16_t)(ax_lp >> 4);
        int64_t mag_sq  = (int64_t)ax_dyn * ax_dyn;

        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        // ── Timeout: sem pico válido por RUN_TIMEOUT_MS ──────────────────────
        if (state == STATE_RUNNING && last_valid_ms > 0 &&
            (now_ms - last_valid_ms) > RUN_TIMEOUT_MS) {

            ESP_LOGI(TAG, "Corrida encerrada (timeout)");
            state      = STATE_IDLE;
            ring_count = 0;
            ring_head  = 0;
            ring_sum   = 0;
            if (s_motion_cb) s_motion_cb(MPU_EVENT_STOPPED, 0);
        }

        // ── Peak detection com histerese ─────────────────────────────────────
        if (!above && mag_sq > HIGH_SQ) {
            above = true;

            if (last_peak_ms > 0) {
                uint32_t interval = now_ms - last_peak_ms;

                // Critério 1 — cadência dentro da faixa de corrida
                bool cadence_ok = (interval >= RUN_MIN_INTERVAL_MS &&
                                   interval <= RUN_MAX_INTERVAL_MS);

                // Critério 2 — ritmo consistente com os últimos picos
                bool rhythm_ok = true;
                if (cadence_ok && ring_count > 0) {
                    uint32_t avg = ring_sum / ring_count;
                    uint32_t tol = avg * RUN_RHYTHM_TOL_PCT / 100;
                    rhythm_ok = (interval + tol >= avg) && (interval <= avg + tol);
                }

                if (cadence_ok && rhythm_ok) {
                    // Pico válido: insere no ring buffer
                    if (ring_count < RUN_CONFIRM_STEPS) {
                        ring[ring_head] = interval;
                        ring_sum       += interval;
                        ring_count++;
                    } else {
                        // Buffer cheio: remove o mais antigo e insere o novo
                        ring_sum          -= ring[ring_head];
                        ring[ring_head]    = interval;
                        ring_sum          += interval;
                    }
                    ring_head = (ring_head + 1) % RUN_CONFIRM_STEPS;
                    last_valid_ms = now_ms;

                    uint32_t avg     = ring_sum / ring_count;
                    uint32_t cadence = 60000u / avg;

                    ESP_LOGD(TAG, "Pico valido: interval=%lums  avg=%lums  cadencia=%luspm  n=%u",
                             (unsigned long)interval, (unsigned long)avg,
                             (unsigned long)cadence, ring_count);

                    if (ring_count >= RUN_CONFIRM_STEPS) {
                        if (state == STATE_CONFIRMING) {
                            state = STATE_RUNNING;
                            ESP_LOGI(TAG, "Corrida confirmada: cadencia=%lu spm", (unsigned long)cadence);
                        }
                        if (state == STATE_RUNNING && s_motion_cb) {
                            s_motion_cb(MPU_EVENT_RUNNING, cadence);
                        }
                    } else {
                        state = STATE_CONFIRMING;
                    }

                } else {
                    // Pico inválido: reseta acumulação
                    ESP_LOGD(TAG, "Pico descartado: interval=%lums  cadence_ok=%d  rhythm_ok=%d",
                             (unsigned long)interval, cadence_ok, rhythm_ok);

                    bool was_running = (state == STATE_RUNNING);
                    state      = STATE_IDLE;
                    ring_count = 0;
                    ring_head  = 0;
                    ring_sum   = 0;

                    if (was_running && s_motion_cb) s_motion_cb(MPU_EVENT_STOPPED, 0);
                }
            }

            last_peak_ms = now_ms;

        } else if (above && mag_sq < LOW_SQ) {
            above = false;
        }
    }
}

// ─── Configuração dos registradores ──────────────────────────────────────────

static esp_err_t mpu6050_configure(void)
{
    esp_err_t err;

    err = mpu_write(MPU_REG_PWR_MGMT_1, 0x00);  // acorda o sensor
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(10));

    // 1 kHz / (19 + 1) = 50 Hz
    err = mpu_write(MPU_REG_SMPLRT_DIV, 0x13);
    if (err != ESP_OK) return err;

    // DLPF = 2: BW 98 Hz — filtra ruído elétrico, preserva transientes de passo
    err = mpu_write(MPU_REG_CONFIG, 0x02);
    if (err != ESP_OK) return err;

    // ±2 g → 16384 LSB/g (máxima sensibilidade)
    err = mpu_write(MPU_REG_ACCEL_CONFIG, 0x00);
    if (err != ESP_OK) return err;

    // INT ativo alto, push-pull, nível mantido até INT_STATUS ser lido
    err = mpu_write(MPU_REG_INT_PIN_CFG, 0x20);
    if (err != ESP_OK) return err;

    // DATA_RDY interrupt (bit 0)
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
    return val;  // 0x68 = OK
}

esp_err_t mpu6050_init(mpu6050_motion_cb_t on_motion)
{
    s_motion_cb = on_motion;

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

    // Limpa interrupção pendente antes de registrar o ISR de borda.
    // Com LATCH_INT_EN=1 o pino INT pode já estar HIGH; sem esta leitura
    // o POSEDGE nunca dispararia e a task bloquearia para sempre.
    uint8_t dummy;
    mpu_read(MPU_REG_INT_STATUS, &dummy, 1);

    xTaskCreate(mpu6050_step_task, "mpu6050_step", 3072, NULL, 5, &s_task_handle);

    err = mpu6050_gpio_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro na GPIO %d: %s", MPU6050_INT_GPIO, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Detector de corrida ativo: impacto=%.2fg  cadencia=%d-%dspm  confirmacao=%d passos",
             STEP_HIGH_G_LSB / 16384.0f,
             60000 / RUN_MAX_INTERVAL_MS,
             60000 / RUN_MIN_INTERVAL_MS,
             RUN_CONFIRM_STEPS);
    return ESP_OK;
}

void mpu6050_suspend(void) { if (s_task_handle) vTaskSuspend(s_task_handle); }
void mpu6050_resume(void)  { if (s_task_handle) vTaskResume(s_task_handle);  }
