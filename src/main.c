#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "max30105.h"
#include "spo2_algorithm.h"

static const char *TAG = "MAX30102";

// ==================== CONFIGURAÇÃO I2C ====================
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_FREQ_HZ 100000

// ==================== CONFIGURAÇÃO MAX30102 ====================
#define MAX30105_ADDR 0x57
#define LED_POWER 0x5F              // ~19 mA — melhor amplitude AC para SpO2
#define SAMPLE_AVERAGE 4            // Média de 4 amostras (padrão recomendado)
#define LED_MODE 2                  // Modo LED: 0=Red only, 1=Red+IR, 2=Red+IR+Green
#define SAMPLE_RATE 100             // Taxa nativa do MAX30102 (100 Hz)
#define PULSE_WIDTH 411             // Largura do pulso (máxima resolução)
#define ADC_RANGE 4096              // Faixa ADC 18-bit

// ==================== CONFIGURAÇÃO DO ALGORITMO ====================
#define MAX_BUFFER_SIZE 100              // 100 amostras EXATAS (compatível com Maxim algorithm)
#define UPDATE_INTERVAL 25               // Atualizar a cada 25 amostras (sliding window)
#define SMOOTHING_SAMPLES 0              // SEM suavização adicional (algoritmo já faz filtragem)

// ==================== BUFFERS ====================
static uint32_t aun_ir_buffer[MAX_BUFFER_SIZE];
static uint32_t aun_red_buffer[MAX_BUFFER_SIZE];
static uint32_t n_sample_count = 0;

// ==================== RESULTADOS ====================
static int32_t n_spo2 = 0;
static int8_t ch_spo2_valid = 0;
static int32_t n_heart_rate = 0;
static int8_t ch_hr_valid = 0;
static uint32_t last_update = 0;

// ==================== FILTRO DE MEDIANA (para HR) ====================
static int32_t hr_history[5] = {0};
static uint8_t hr_hist_idx = 0;
static int32_t spo2_history[5] = {0};
static uint8_t spo2_hist_idx = 0;

// ==================== FUNÇÕES AUXILIARES ====================

/**
 * @brief Obtém mediana de um array (para filtrar valores anômalos)
 */
static int32_t get_median(int32_t *values, uint8_t count) {
    // Bubble sort simples
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - 1 - i; j++) {
            if (values[j] > values[j + 1]) {
                int32_t temp = values[j];
                values[j] = values[j + 1];
                values[j + 1] = temp;
            }
        }
    }
    return values[count / 2];
}

/**
 * @brief Adiciona valor ao histórico e retorna mediana
 */
static int32_t apply_median_filter(int32_t new_value, int32_t *history, uint8_t *index) {
    history[*index] = new_value;
    *index = (*index + 1) % 5;
    
    int32_t temp[5];
    memcpy(temp, history, 5 * sizeof(int32_t));
    return get_median(temp, 5);
}

/**
 * @brief Valida se o valor está dentro do range aceitável
 */
static bool is_value_valid(int32_t value, int32_t min_val, int32_t max_val) {
    return (value >= min_val && value <= max_val);
}

// ==================== INICIALIZAÇÃO I2C ====================
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao configurar I2C: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao instalar driver I2C: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C inicializado com sucesso");
    return ESP_OK;
}

// ==================== INICIALIZAÇÃO MAX30102 ====================
static bool max30102_init_sensor(max30105_t *sensor) {
    if (!max30105_init(sensor, I2C_MASTER_NUM, MAX30105_ADDR)) {
        ESP_LOGE(TAG, "Erro: MAX30102/MAX30105 não encontrado no endereço 0x%02X", MAX30105_ADDR);
        return false;
    }

    ESP_LOGI(TAG, "MAX30102/MAX30105 detectado com sucesso");

    // Configurar o sensor
    max30105_setup(sensor, LED_POWER, SAMPLE_AVERAGE, LED_MODE, SAMPLE_RATE, PULSE_WIDTH, ADC_RANGE);
    
    ESP_LOGI(TAG, "Sensor configurado:");
    ESP_LOGI(TAG, "  - Potência LED: 0x%02X", LED_POWER);
    ESP_LOGI(TAG, "  - Taxa de amostragem: %d Hz", SAMPLE_RATE);
    ESP_LOGI(TAG, "  - Modo LED: RED+IR");
    
    return true;
}

// ==================== PROCESSAMENTO DE DADOS ====================
// Computa range (max-min) do buffer IR para detectar motion artifact
static uint32_t ir_signal_range(void) {
    uint32_t vmin = aun_ir_buffer[0], vmax = aun_ir_buffer[0];
    for (int i = 1; i < 100; i++) {
        if (aun_ir_buffer[i] < vmin) vmin = aun_ir_buffer[i];
        if (aun_ir_buffer[i] > vmax) vmax = aun_ir_buffer[i];
    }
    return vmax - vmin;
}

static void process_buffer_data(void) {
    if (n_sample_count < 100) return;

    // ==================== QUALIDADE DE SINAL ====================

    uint32_t ir_dc = aun_ir_buffer[99]; // última amostra como proxy do DC

    // Dedo ausente: IR abaixo de 50.000 indica sem contato
    if (ir_dc < 50000) {
        printf("\n[AGUARDANDO] Coloque o dedo no sensor (IR=%lu)\n", ir_dc);
        return;
    }

    // Range do IR nos últimos 100 samples (4 segundos a 25 Hz)
    uint32_t ir_range = ir_signal_range();
    // < 300 → sinal muito fraco (pressão insuficiente)
    // > 15000 → artefato de movimento
    if (ir_range < 300) {
        printf("\n[SINAL FRACO] Pressione mais o dedo (range IR=%lu)\n", ir_range);
        return;
    }
    if (ir_range > 15000) {
        printf("\n[MOVIMENTO] Mantenha o dedo parado (range IR=%lu)\n", ir_range);
        return;
    }

    // ==================== ALGORITMO ====================

    int32_t temp_spo2 = 0;
    int8_t  temp_spo2_valid = 0;
    int32_t temp_hr = 0;
    int8_t  temp_hr_valid = 0;

    maxim_heart_rate_and_oxygen_saturation(
        aun_ir_buffer, 100, aun_red_buffer,
        &temp_spo2, &temp_spo2_valid,
        &temp_hr,   &temp_hr_valid
    );

    // ==================== VALIDAÇÕES ====================

    if (temp_spo2_valid && is_value_valid(temp_spo2, 85, 100)) {
        n_spo2 = temp_spo2;
        ch_spo2_valid = 1;
    } else {
        ch_spo2_valid = 0;
        n_spo2 = 0;
    }

    if (temp_hr_valid && is_value_valid(temp_hr, 40, 180)) {
        n_heart_rate = apply_median_filter(temp_hr, hr_history, &hr_hist_idx);
        ch_hr_valid = 1;
    } else {
        ch_hr_valid = 0;
        n_heart_rate = 0;
    }

    // ==================== EXIBIÇÃO ====================
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║     LEITURA MAX30102 - SPO2 & HR      ║\n");
    printf("╠════════════════════════════════════════╣\n");

    if (ch_spo2_valid) {
        printf("║ SpO2: %3ld%% [VALIDO]                  ║\n", n_spo2);
    } else {
        // Mostra valor bruto para diagnóstico (-999 = ratio fora de range)
        printf("║ SpO2: N/A [raw=%4ld] range IR=%5lu ║\n", temp_spo2, ir_range);
    }

    if (ch_hr_valid) {
        printf("║ HR:   %3ld BPM [VALIDO]               ║\n", n_heart_rate);
    } else {
        printf("║ HR:   N/A [raw=%4ld]                 ║\n", temp_hr);
    }

    printf("╚════════════════════════════════════════╝\n");
}

// ==================== THREAD PRINCIPAL ====================
void app_main(void) {
    ESP_LOGI(TAG, "=== INICIANDO APLICAÇÃO MAX30102 ===");
    
    // Inicializar I2C
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar I2C. Abortar.");
        return;
    }

    // Pequena pausa para estabilização
    vTaskDelay(pdMS_TO_TICKS(100));

    // Inicializar sensor
    max30105_t sensor;
    if (!max30102_init_sensor(&sensor)) {
        ESP_LOGE(TAG, "Falha ao inicializar MAX30102. Abortar.");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Iniciando coleta de dados...");
    printf("\n=== COLETANDO DADOS DO MAX30102 ===\n");
    printf("Aguardando %d amostras para cálculo inicial...\n\n", MAX_BUFFER_SIZE);

    // Loop principal
    while (1) {
        // Carrega todas as amostras novas do sensor no ring buffer interno
        max30105_check(&sensor);

        // Drena todas as amostras disponíveis antes de dormir
        while (max30105_available(&sensor)) {
            uint32_t ir_value  = max30105_get_ir(&sensor);
            uint32_t red_value = max30105_get_red(&sensor);
            max30105_next_sample(&sensor);

            // Armazenar no buffer circular (sliding window)
            if (n_sample_count < 100) {
                aun_ir_buffer[n_sample_count]  = ir_value;
                aun_red_buffer[n_sample_count] = red_value;
            } else {
                memmove(aun_ir_buffer,  &aun_ir_buffer[1],  99 * sizeof(uint32_t));
                memmove(aun_red_buffer, &aun_red_buffer[1], 99 * sizeof(uint32_t));
                aun_ir_buffer[99]  = ir_value;
                aun_red_buffer[99] = red_value;
            }

            n_sample_count++;

            if (n_sample_count % 25 == 0) {
                printf("[%5lu] IR: %8lu | RED: %8lu\n", n_sample_count, ir_value, red_value);
            }

            if (n_sample_count >= 100 && (n_sample_count - 100) % UPDATE_INTERVAL == 0) {
                process_buffer_data();
                last_update = n_sample_count;
            }
        }

        // Delay de 10 ms — com 25 Hz efetivo, amostras chegam a cada 40 ms
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
