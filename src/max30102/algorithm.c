#include "algorithm.h"
#include <math.h>
#include <string.h>

// Razão mínima de autocorrelação para considerar um pico válido
#define MINIMUM_RATIO  0.2

static double time_array[BUFFER_SIZE];

// Pré-calcula o eixo de tempo (0, 0.04, 0.08, ... segundos)
void init_time_array(void)
{
    double t = 0.0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        time_array[i] = t;
        t += DELAY_AMOSTRAGEM / 1000.0;
    }
}

// Remove a componente DC (média) de ambos os sinais e retorna as médias
void remove_dc_part(int32_t *ir_buffer, int32_t *red_buffer,
                    uint64_t *ir_mean, uint64_t *red_mean)
{
    *ir_mean  = 0;
    *red_mean = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        *ir_mean  += (uint64_t)ir_buffer[i];
        *red_mean += (uint64_t)red_buffer[i];
    }
    *ir_mean  /= BUFFER_SIZE;
    *red_mean /= BUFFER_SIZE;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        ir_buffer[i]  -= (int32_t)(*ir_mean);
        red_buffer[i] -= (int32_t)(*red_mean);
    }
}

// Remove a tendência linear (drift de baseline) do sinal
void remove_trend_line(int32_t *buffer)
{
    double a = 0.0, b = 0.0;
    calculate_linear_regression(&a, &b, buffer);

    double t = 0.0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer[i] = (int32_t)((buffer[i] - a * t) - b);
        t += DELAY_AMOSTRAGEM / 1000.0;
    }
}

// Regressão linear pelo método dos mínimos quadrados
// Fonte: https://www.statisticshowto.com/find-a-linear-regression-equation/
void calculate_linear_regression(double *angular_coef, double *linear_coef,
                                  int32_t *data)
{
    // Σx para t = 0..127 com passo 0.04 s → 0.04 * (127*128/2) = 325.12
    const double sum_of_x  = 325.12;
    int64_t      sum_of_y  = sum_of_elements(data);
    double       sum_of_x2 = somatoria_x2();
    double       sum_of_xy = sum_of_xy_elements(data);

    double temp  = sum_of_xy - (sum_of_x * (double)sum_of_y / BUFFER_SIZE);
    double temp2 = sum_of_x2 - (sum_of_x * sum_of_x / BUFFER_SIZE);

    *angular_coef = temp / temp2;
    *linear_coef  = ((double)sum_of_y / BUFFER_SIZE)
                  - (*angular_coef * (sum_of_x / BUFFER_SIZE));
}

// Correlação de Pearson entre RED e IR (indica qualidade do sinal PPG)
double correlation_datay_datax(int32_t *data_red, int32_t *data_ir)
{
    double sum_x = 0.0, sum_y = 0.0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        sum_x += data_red[i];
        sum_y += data_ir[i];
    }
    double x_mean = sum_x / BUFFER_SIZE;
    double y_mean = sum_y / BUFFER_SIZE;

    double cov = 0.0, sx2 = 0.0, sy2 = 0.0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        double dx = data_red[i] - x_mean;
        double dy = data_ir[i]  - y_mean;
        cov += dx * dy;
        sx2 += dx * dx;
        sy2 += dy * dy;
    }
    if (sx2 == 0.0 || sy2 == 0.0) return 0.0;
    return (cov / BUFFER_SIZE) / (sqrt(sx2 / BUFFER_SIZE) * sqrt(sy2 / BUFFER_SIZE));
}

// Calcula SpO2 via razão RMS/DC dos canais RED e IR
// Fórmula quadrática Maxim: SpO2 = -45.06*R² + 30.354*R + 94.845
double spo2_measurement(int32_t *ir_data, int32_t *red_data,
                        uint64_t ir_mean, uint64_t red_mean)
{
    double ir_rms  = rms_value(ir_data);
    double red_rms = rms_value(red_data);

    if (ir_mean == 0 || red_mean == 0 || red_rms == 0.0) return -1.0;

    // Este sensor clone apresenta relação AC/DC invertida em relação ao
    // sensor genuíno: o canal IR exibe menor variação AC relativa ao DC
    // do que o RED, ao contrário do esperado pela absorção da oxyHb.
    // Inverte-se o ratio para obter SpO2 fisicamente plausível.
    double R = (ir_rms / (double)ir_mean) / (red_rms / (double)red_mean);

    // Fórmula empírica Maxim (MAXREFDES117)
    double spo2 = (-45.06 * R * R) + (30.354 * R) + 94.845;
    return spo2;
}

// Calcula frequência cardíaca via autocorrelação do sinal IR
// Retorna BPM ou -1 se não encontrar pico válido
int calculate_heart_rate(int32_t *ir_data, double *r0,
                         double *auto_correlationated_data)
{
    double r0_val = auto_correlation_function(ir_data, 0);
    *r0 = r0_val;

    if (r0_val == 0.0) return -1;

    double biggest_value = 0.0;
    int    biggest_index = 0;

    for (int lag = 1; lag < 125; lag++) {
        double acf     = auto_correlation_function(ir_data, lag);
        double normed  = acf / r0_val;
        auto_correlationated_data[lag] = normed;

        // lag > 14 evita detectar 2º harmônico (lag=11 → 136 bpm para FC real ≈ 68 bpm)
        if (lag > 14) {
            if (normed > MINIMUM_RATIO) {
                if (normed > biggest_value) {
                    biggest_value = normed;
                    biggest_index = lag;
                } else {
                    // Passou o pico: calcula HR e retorna
                    if (biggest_index > 0) {
                        double periodo_s = biggest_index * (DELAY_AMOSTRAGEM / 1000.0);
                        return (int)(60.0 / periodo_s);
                    }
                }
            }
        }
    }

    // Pico encontrado mas sem descida antes do fim do buffer
    if (biggest_index > 0) {
        double periodo_s = biggest_index * (DELAY_AMOSTRAGEM / 1000.0);
        return (int)(60.0 / periodo_s);
    }
    return -1;
}

// Autocorrelação com deslocamento lag
double auto_correlation_function(int32_t *data, int32_t lag)
{
    double soma = 0.0;
    for (int i = 0; i < (BUFFER_SIZE - lag); i++) {
        soma += (double)data[i] * (double)data[i + lag];
    }
    return soma / BUFFER_SIZE;
}

int64_t sum_of_elements(int32_t *data)
{
    int64_t sum = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) sum += data[i];
    return sum;
}

double sum_of_xy_elements(int32_t *data)
{
    double sum_xy = 0.0;
    double t = 0.0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        sum_xy += data[i] * t;
        t += DELAY_AMOSTRAGEM / 1000.0;
    }
    return sum_xy;
}

double sum_of_squared_elements(int32_t *data)
{
    double sum = 0.0;
    for (int i = 0; i < BUFFER_SIZE; i++) sum += (double)data[i] * data[i];
    return sum;
}

// Σ(t²) para t = 0, 0.04, ..., (BUFFER_SIZE-1)*0.04 — constante, calculada uma vez
double somatoria_x2(void)
{
    static double cached = 0.0;
    static int    done   = 0;
    if (done) return cached;

    double t = 0.0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        cached += t * t;
        t += DELAY_AMOSTRAGEM / 1000.0;
    }
    done = 1;
    return cached;
}

// RMS do sinal AC (usa int64 para evitar overflow com 128 amostras)
double rms_value(int32_t *data)
{
    int64_t sum_sq = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        sum_sq += (int64_t)data[i] * data[i];
    }
    return sqrt((double)sum_sq / BUFFER_SIZE);
}
