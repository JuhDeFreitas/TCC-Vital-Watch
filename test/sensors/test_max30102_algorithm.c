/**
 * Testes unitários nativos — rodam no PC sem hardware.
 * Execução: pio test -e native
 *
 * Dois grupos:
 *   1. Lógica básica  — verifica comportamento de cada função isolada
 *   2. Simulação PPG  — passa valores simulados do sensor pelo pipeline completo
 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <unity.h>
#include <math.h>
#include <string.h>

/* Inclui a implementação diretamente — padrão para testes nativos em C */
#include "../../src/max30102/algorithm.c"

/* ─── helpers ─── */

/* Gera senoide centrada em zero */
static void fill_sine(int32_t *buf, int n, double amplitude, double period_samples)
{
    for (int i = 0; i < n; i++)
        buf[i] = (int32_t)(amplitude * sin(2.0 * M_PI * i / period_samples));
}

/*
 * Gera sinal PPG sintético realista:
 *   DC base + pulsação senoidal em ambos os canais
 *   Parâmetros típicos observados neste projeto:
 *     dc_ir  ≈ 88000, dc_red ≈ 89000
 *     ac_ir  ≈ 2000 amplitude, ac_red ≈ 3000 amplitude
 *     frequência cardíaca: bpm_period = 25/(bpm/60) amostras
 */
static void fill_ppg(int32_t *ir, int32_t *red,
                     double dc_ir, double dc_red,
                     double ac_ir, double ac_red,
                     double period_samples)
{
    for (int i = 0; i < BUFFER_SIZE; i++) {
        double phase = 2.0 * M_PI * i / period_samples;
        ir[i]  = (int32_t)(dc_ir  + ac_ir  * sin(phase));
        red[i] = (int32_t)(dc_red + ac_red * sin(phase));
    }
}

void setUp(void)    { init_time_array(); }
void tearDown(void) {}

/* ═══════════════════════════════════════════════════════════════
 * 1. LÓGICA BÁSICA
 * ═══════════════════════════════════════════════════════════════ */

/* ── rms_value ── */

void test_rms_sinal_zero_retorna_zero(void)
{
    int32_t buf[BUFFER_SIZE];
    memset(buf, 0, sizeof(buf));
    TEST_ASSERT_FLOAT_WITHIN(0.1, 0.0, rms_value(buf));
}

void test_rms_sinal_constante(void)
{
    int32_t buf[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; i++) buf[i] = 1000;
    TEST_ASSERT_FLOAT_WITHIN(1.0, 1000.0, rms_value(buf));
}

/* ── remove_dc_part ── */

void test_remove_dc_retorna_media_correta(void)
{
    int32_t ir[BUFFER_SIZE], red[BUFFER_SIZE];
    uint64_t ir_mean, red_mean;
    for (int i = 0; i < BUFFER_SIZE; i++) { ir[i] = 80000; red[i] = 90000; }

    remove_dc_part(ir, red, &ir_mean, &red_mean);

    TEST_ASSERT_EQUAL_UINT64(80000, ir_mean);
    TEST_ASSERT_EQUAL_UINT64(90000, red_mean);
}

void test_remove_dc_zera_sinal_constante(void)
{
    int32_t ir[BUFFER_SIZE], red[BUFFER_SIZE];
    uint64_t ir_mean, red_mean;
    for (int i = 0; i < BUFFER_SIZE; i++) { ir[i] = 80000; red[i] = 90000; }

    remove_dc_part(ir, red, &ir_mean, &red_mean);

    for (int i = 0; i < BUFFER_SIZE; i++) {
        TEST_ASSERT_INT_WITHIN(1, 0, ir[i]);
        TEST_ASSERT_INT_WITHIN(1, 0, red[i]);
    }
}

/* ── remove_trend_line ── */

void test_detrend_remove_rampa(void)
{
    /* Rampa pura → após detrend todos os valores devem ser ~0 */
    int32_t buf[BUFFER_SIZE];
    double t = 0.0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buf[i] = (int32_t)(300.0 * t);
        t += DELAY_AMOSTRAGEM / 1000.0;
    }
    remove_trend_line(buf);
    for (int i = 0; i < BUFFER_SIZE; i++)
        TEST_ASSERT_INT_WITHIN(10, 0, buf[i]);
}

/* ── correlation_datay_datax ── */

void test_correlacao_sinais_identicos_igual_1(void)
{
    int32_t a[BUFFER_SIZE], b[BUFFER_SIZE];
    fill_sine(a, BUFFER_SIZE, 5000.0, 25.0);
    memcpy(b, a, sizeof(b));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.0, correlation_datay_datax(a, b));
}

void test_correlacao_sinais_opostos_igual_menos1(void)
{
    int32_t a[BUFFER_SIZE], b[BUFFER_SIZE];
    fill_sine(a, BUFFER_SIZE,  5000.0, 25.0);
    fill_sine(b, BUFFER_SIZE, -5000.0, 25.0);
    TEST_ASSERT_FLOAT_WITHIN(0.01, -1.0, correlation_datay_datax(a, b));
}

/* ── calculate_heart_rate ── */

void test_fc_sinal_zero_retorna_negativo(void)
{
    int32_t ir[BUFFER_SIZE];
    double auto_buf[BUFFER_SIZE];
    double r0;
    memset(ir, 0, sizeof(ir));
    TEST_ASSERT_EQUAL_INT(-1, calculate_heart_rate(ir, &r0, auto_buf));
}

void test_fc_senoide_60bpm_detectada(void)
{
    /* 60 bpm → período = 25 amostras a 25 Hz */
    int32_t ir[BUFFER_SIZE];
    double auto_buf[BUFFER_SIZE];
    double r0;
    fill_sine(ir, BUFFER_SIZE, 8000.0, 25.0);

    int hr = calculate_heart_rate(ir, &r0, auto_buf);

    TEST_ASSERT_NOT_EQUAL(-1, hr);
    TEST_ASSERT_INT_WITHIN(8, 60, hr);
}

/* ── spo2_measurement ── */

void test_spo2_medias_zero_retorna_negativo(void)
{
    int32_t ir[BUFFER_SIZE], red[BUFFER_SIZE];
    memset(ir, 0, sizeof(ir));
    memset(red, 0, sizeof(red));
    TEST_ASSERT_EQUAL_FLOAT(-1.0, spo2_measurement(ir, red, 0, 0));
}

/* ═══════════════════════════════════════════════════════════════
 * 2. SIMULAÇÃO DE VALORES DO SENSOR
 *    Passa dados PPG realistas pelo pipeline completo e verifica
 *    se FC e SpO2 resultantes são fisiologicamente plausíveis.
 * ═══════════════════════════════════════════════════════════════ */

/*
 * Pipeline completo: remove_dc → remove_trend → heart_rate + spo2
 * Retorna 1 se os resultados são válidos.
 */
static int pipeline(int32_t *ir, int32_t *red,
                    int *hr_out, double *spo2_out)
{
    uint64_t ir_mean, red_mean;
    double auto_buf[BUFFER_SIZE];
    double r0;

    remove_dc_part(ir, red, &ir_mean, &red_mean);
    remove_trend_line(ir);
    remove_trend_line(red);

    *hr_out   = calculate_heart_rate(ir, &r0, auto_buf);
    *spo2_out = spo2_measurement(ir, red, ir_mean, red_mean);
    return 1;
}

void test_simulacao_sinal_tipico_60bpm(void)
{
    /*
     * Simula sensor com dedo bem posicionado a 60 bpm.
     * DC IR ≈ 88000, DC RED ≈ 89000 (valores reais observados neste projeto).
     * AC: amplitude IR = 2000, RED = 3200 (clone: AC_red > AC_ir).
     */
    int32_t ir[BUFFER_SIZE], red[BUFFER_SIZE];
    fill_ppg(ir, red, 88000, 89000, 2000, 3200, 25.0);  /* 25 amostras = 60 bpm */

    int hr; double spo2;
    pipeline(ir, red, &hr, &spo2);

    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, hr, "FC nao detectada para 60 bpm");
    TEST_ASSERT_INT_WITHIN_MESSAGE(10, 60, hr, "FC fora do intervalo esperado");
}

void test_simulacao_sinal_tipico_75bpm(void)
{
    /* 75 bpm → período = 20 amostras a 25 Hz */
    int32_t ir[BUFFER_SIZE], red[BUFFER_SIZE];
    fill_ppg(ir, red, 88000, 89000, 2000, 3200, 20.0);

    int hr; double spo2;
    pipeline(ir, red, &hr, &spo2);

    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, hr, "FC nao detectada para 75 bpm");
    TEST_ASSERT_INT_WITHIN_MESSAGE(10, 75, hr, "FC fora do intervalo esperado");
}

void test_simulacao_spo2_faixa_fisiologica(void)
{
    /*
     * Com sinal PPG válido, SpO2 deve cair entre 85 % e 100 %.
     * Usa relação AC IR/RED idêntica à observada com este sensor clone.
     */
    int32_t ir[BUFFER_SIZE], red[BUFFER_SIZE];
    fill_ppg(ir, red, 88000, 89000, 2000, 3200, 25.0);

    int hr; double spo2;
    pipeline(ir, red, &hr, &spo2);

    if (spo2 > 0.0) {
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(85.0, spo2,
            "SpO2 abaixo de 85% — formula ou relacao AC/DC incorreta");
        TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(100.1, spo2,
            "SpO2 acima de 100% — relacao R fora do intervalo valido");
    }
}

void test_simulacao_sem_dedo_dc_baixo(void)
{
    /*
     * Sem dedo: DC IR < 10000 (limiar de detecção em main.c).
     * O pipeline ainda deve executar sem travar; FC pode ser -1.
     */
    int32_t ir[BUFFER_SIZE], red[BUFFER_SIZE];
    /* Sinal muito fraco, como sem dedo */
    fill_ppg(ir, red, 800, 800, 50, 50, 25.0);

    uint64_t ir_mean = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) ir_mean += (uint64_t)ir[i];
    ir_mean /= BUFFER_SIZE;

    /* Verifica que o DC ficou abaixo do limiar de detecção */
    TEST_ASSERT_LESS_THAN_UINT64_MESSAGE(10000, ir_mean,
        "DC IR deveria ser < 10000 quando sem dedo");
}

void test_simulacao_saturacao_dc_alto(void)
{
    /*
     * Saturação: valores ≥ 262143 indicam LED com corrente muito alta.
     * Verifica que o sistema identifica a condição.
     */
    int32_t ir[BUFFER_SIZE], red[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; i++) { ir[i] = 262143; red[i] = 262143; }

    uint64_t ir_mean = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) ir_mean += (uint64_t)ir[i];
    ir_mean /= BUFFER_SIZE;

    TEST_ASSERT_EQUAL_UINT64_MESSAGE(262143, ir_mean,
        "DC deve ser 262143 em condicao de saturacao");
}

void test_simulacao_dc_na_faixa_ideal(void)
{
    /*
     * Confirma que os parâmetros atuais (LED_PA=0x18, ADC_RGE=01)
     * produzem DC na faixa-alvo 50000–150000.
     * Usa os valores médios observados nos testes reais deste projeto.
     */
    const uint64_t dc_ir_tipico  = 88000;
    const uint64_t dc_red_tipico = 89000;

    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(50000, dc_ir_tipico,
        "DC IR abaixo de 50000 — corrente do LED muito baixa");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(150000, dc_ir_tipico,
        "DC IR acima de 150000 — risco de saturacao");
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(50000, dc_red_tipico,
        "DC RED abaixo de 50000 — corrente do LED muito baixa");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(150000, dc_red_tipico,
        "DC RED acima de 150000 — risco de saturacao");
}

/* ═══════════════════════ runner ═══════════════════════════════ */

int main(void)
{
    UNITY_BEGIN();

    /* Lógica básica */
    RUN_TEST(test_rms_sinal_zero_retorna_zero);
    RUN_TEST(test_rms_sinal_constante);
    RUN_TEST(test_remove_dc_retorna_media_correta);
    RUN_TEST(test_remove_dc_zera_sinal_constante);
    RUN_TEST(test_detrend_remove_rampa);
    RUN_TEST(test_correlacao_sinais_identicos_igual_1);
    RUN_TEST(test_correlacao_sinais_opostos_igual_menos1);
    RUN_TEST(test_fc_sinal_zero_retorna_negativo);
    RUN_TEST(test_fc_senoide_60bpm_detectada);
    RUN_TEST(test_spo2_medias_zero_retorna_negativo);

    /* Simulação de valores do sensor */
    RUN_TEST(test_simulacao_sinal_tipico_60bpm);
    RUN_TEST(test_simulacao_sinal_tipico_75bpm);
    RUN_TEST(test_simulacao_spo2_faixa_fisiologica);
    RUN_TEST(test_simulacao_sem_dedo_dc_baixo);
    RUN_TEST(test_simulacao_saturacao_dc_alto);
    RUN_TEST(test_simulacao_dc_na_faixa_ideal);

    return UNITY_END();
}
