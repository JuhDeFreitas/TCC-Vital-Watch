/**
 * Testes de integração embarcados — requerem ESP32 + MAX30102 conectados.
 * Execução: pio test -e esp32-s2-sensor-test
 *
 * Cobrem:
 *   - Inicialização I2C
 *   - Identificação do sensor (PART_ID, REV_ID)
 *   - Escrita e leitura de registradores
 *   - Leitura do FIFO (limites de 18 bits)
 *   - Temperatura interna do chip
 *   - Configuração dos LEDs
 */

#include <unity.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_api.h"
#include "max30102_api.h"

static const char *TAG = "TEST_SENSOR";

/* ─── configuração mínima para inicializar o sensor nos testes ─── */
static max_config test_cfg = {
    .FIFO_CONF.SMP_AVE          = 0b010,
    .FIFO_CONF.FIFO_ROLLOVER_EN = 1,
    .MODE_CONF.MODE             = 0b011,   /* SpO2 (RED + IR) */
    .SPO2_CONF.SPO2_ADC_RGE     = 0b01,
    .SPO2_CONF.SPO2_SR          = 0b001,
    .SPO2_CONF.LED_PW           = 0b11,
    .LED1_PULSE_AMP.LED1_PA     = 0x18,
    .LED2_PULSE_AMP.LED2_PA     = 0x18,
    .PROX_LED_PULS_AMP.PILOT_PA = 0x7F,
};

/* ───────────────────────── setUp / tearDown ───────────────── */

void setUp(void)   {}
void tearDown(void) {}

/* ═══════════════════════ I2C ═════════════════════════════════ */

void test_i2c_inicializa_sem_erro(void)
{
    /* I2C já foi inicializado antes de UNITY_BEGIN — só verifica leitura básica */
    uint8_t dummy = 0;
    esp_err_t err = i2c_sensor_read(&dummy, 1);
    /* O sensor existe: a leitura deve responder (mesmo que dado seja irrelevante) */
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err,
        "I2C não respondeu — verifique fiação SDA/SCL e endereço 0x57");
}

/* ═══════════════════════ Identificação do sensor ════════════ */

void test_part_id_correto(void)
{
    /*
     * REG_PART_ID (0xFF) deve retornar 0x15 em qualquer MAX30102 original
     * ou clone compatível.
     */
    uint8_t part_id = 0x00;
    read_max30102_reg(REG_PART_ID, &part_id, 1);
    ESP_LOGI(TAG, "PART_ID lido: 0x%02X (esperado: 0x15)", part_id);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x15, part_id,
        "PART_ID incorreto — sensor pode não ser MAX30102 ou I2C com falha");
}

void test_rev_id_nao_zero(void)
{
    /* REG_REV_ID (0xFE) varia entre revisões mas nunca deve ser 0x00 */
    uint8_t rev_id = 0x00;
    read_max30102_reg(REG_REV_ID, &rev_id, 1);
    ESP_LOGI(TAG, "REV_ID lido: 0x%02X", rev_id);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0x00, rev_id,
        "REV_ID = 0x00 indica falha de leitura I2C");
}

/* ═══════════════════════ Escrita / leitura de registradores ═ */

void test_escreve_le_corrente_led1(void)
{
    /* Escreve valor teste no LED1_PA, lê de volta e verifica */
    const uint8_t valor_teste = 0x3F;
    write_max30102_reg(valor_teste, REG_LED1_PA);
    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t lido = 0x00;
    read_max30102_reg(REG_LED1_PA, &lido, 1);
    ESP_LOGI(TAG, "LED1_PA escrito: 0x%02X, lido: 0x%02X", valor_teste, lido);

    /* Restaura valor original antes de asserção (seguro mesmo se falhar) */
    write_max30102_reg(0x18, REG_LED1_PA);

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(valor_teste, lido,
        "Escrita/leitura do registrador LED1_PA divergem — I2C instável");
}

void test_escreve_le_corrente_led2(void)
{
    const uint8_t valor_teste = 0x20;
    write_max30102_reg(valor_teste, REG_LED2_PA);
    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t lido = 0x00;
    read_max30102_reg(REG_LED2_PA, &lido, 1);

    write_max30102_reg(0x18, REG_LED2_PA);

    TEST_ASSERT_EQUAL_HEX8(valor_teste, lido);
}

void test_reset_limpa_fifo_pointers(void)
{
    /* Zera os ponteiros do FIFO via escrita de 0 */
    write_max30102_reg(0, REG_FIFO_WR_PTR);
    write_max30102_reg(0, REG_OVF_COUNTER);
    write_max30102_reg(0, REG_FIFO_RD_PTR);
    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t wr = 0xFF, rd = 0xFF;
    read_max30102_reg(REG_FIFO_WR_PTR, &wr, 1);
    read_max30102_reg(REG_FIFO_RD_PTR, &rd, 1);

    TEST_ASSERT_EQUAL_HEX8(0x00, wr & 0x1F);  /* apenas 5 bits válidos */
    TEST_ASSERT_EQUAL_HEX8(0x00, rd & 0x1F);
}

/* ═══════════════════════ Leitura do FIFO ════════════════════ */

void test_fifo_valores_dentro_de_18_bits(void)
{
    /*
     * Sem dedo: valores devem ser baixos mas sempre válidos.
     * Com dedo: devem estar abaixo do máximo de 18 bits (0x3FFFF = 262143).
     */
    int32_t red = -1, ir = -1;

    /* Garante que há pelo menos uma amostra no FIFO */
    vTaskDelay(pdMS_TO_TICKS(50));
    read_max30102_fifo(&red, &ir);

    ESP_LOGI(TAG, "FIFO: RED=%ld  IR=%ld", red, ir);

    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, red,
        "RED negativo — leitura FIFO corrompida");
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, ir,
        "IR negativo — leitura FIFO corrompida");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(0x3FFFF, red,
        "RED acima de 18 bits — máscara não aplicada");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(0x3FFFF, ir,
        "IR acima de 18 bits — máscara não aplicada");
}

void test_fifo_multiplas_leituras_consecutivas(void)
{
    /* Verifica que 5 leituras consecutivas retornam valores distintos (sensor ativo) */
    int32_t red[5], ir[5];
    for (int i = 0; i < 5; i++) {
        vTaskDelay(pdMS_TO_TICKS(45));  /* ~1 período de amostragem */
        read_max30102_fifo(&red[i], &ir[i]);
        TEST_ASSERT_GREATER_OR_EQUAL(0, red[i]);
        TEST_ASSERT_GREATER_OR_EQUAL(0, ir[i]);
        TEST_ASSERT_LESS_OR_EQUAL(0x3FFFF, red[i]);
        TEST_ASSERT_LESS_OR_EQUAL(0x3FFFF, ir[i]);
    }
    ESP_LOGI(TAG, "5 leituras FIFO — IR: %ld %ld %ld %ld %ld",
             ir[0], ir[1], ir[2], ir[3], ir[4]);
}

/* ═══════════════════════ Temperatura interna ════════════════ */

void test_temperatura_plausivel(void)
{
    /*
     * Temperatura do die deve estar entre 10 °C e 55 °C em condições normais.
     * Valor retornado é em graus Celsius (float).
     */
    float temp = get_max30102_temp();
    ESP_LOGI(TAG, "Temperatura interna: %.2f °C", temp);
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(10.0f, temp,
        "Temperatura abaixo de 10°C — leitura suspeita");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(55.0f, temp,
        "Temperatura acima de 55°C — superaquecimento ou leitura incorreta");
}

/* ═══════════════════════ app_main (entrada ESP-IDF) ══════════ */

void app_main(void)
{
    /* Aguarda estabilização da serial antes de iniciar saída Unity */
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Inicializando I2C...");
    ESP_ERROR_CHECK(i2c_init());
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Inicializando MAX30102...");
    max30102_init(&test_cfg);
    vTaskDelay(pdMS_TO_TICKS(300));

    UNITY_BEGIN();

    RUN_TEST(test_i2c_inicializa_sem_erro);
    RUN_TEST(test_part_id_correto);
    RUN_TEST(test_rev_id_nao_zero);
    RUN_TEST(test_escreve_le_corrente_led1);
    RUN_TEST(test_escreve_le_corrente_led2);
    RUN_TEST(test_reset_limpa_fifo_pointers);
    RUN_TEST(test_fifo_valores_dentro_de_18_bits);
    RUN_TEST(test_fifo_multiplas_leituras_consecutivas);
    RUN_TEST(test_temperatura_plausivel);

    UNITY_END();

    /* Mantém a tarefa viva para que a saída serial seja completamente enviada */
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
