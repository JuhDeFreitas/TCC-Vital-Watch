#include "sensors/max30102.h"
#include "sensors/max30102_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

/* =====================================================
 * Algorithm configuration
 * ===================================================== */

#define WINDOW_SIZE      128   /* Samples per analysis window                   */
#define SAMPLE_PERIOD_MS  40   /* 40 ms = 25 Hz effective (100 Hz / SMP_AVE 4)  */
#define MIN_ACF_RATIO    0.2   /* Minimum normalised ACF to accept a BPM peak   */
#define MIN_IR_DC     10000u   /* IR DC threshold for finger detection           */

static const char *TAG = "MAX30102";

/* =====================================================
 * Global data (consumed by alert_manager / MQTT layer)
 * ===================================================== */

max30102_data_t g_max_data = {0};

/* =====================================================
 * Static buffers (kept off the task stack)
 * ===================================================== */

static int32_t s_ir[WINDOW_SIZE];
static int32_t s_red[WINDOW_SIZE];
static double  s_acf[WINDOW_SIZE];

static TaskHandle_t s_task_handle = NULL;

/* =====================================================
 * Algorithm helpers
 * ===================================================== */

/* Returns sum of Σ(t²) for t = 0, dt, 2dt, … (N-1)dt — cached after first call. */
static double sum_t2(void)
{
    static double v = 0.0;
    static int    done = 0;
    if (done) return v;
    double t = 0.0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        v += t * t;
        t += SAMPLE_PERIOD_MS / 1000.0;
    }
    done = 1;
    return v;
}

/* Σ(y·t) for the given data array. */
static double sum_yt(int32_t *data)
{
    double s = 0.0, t = 0.0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        s += data[i] * t;
        t += SAMPLE_PERIOD_MS / 1000.0;
    }
    return s;
}

/* Integer sum of all elements. */
static int64_t sum_y(int32_t *data)
{
    int64_t s = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) s += data[i];
    return s;
}

/* Remove DC mean from both channels; returns the saved means. */
static void remove_dc(int32_t *ir, int32_t *red,
                      uint64_t *ir_mean, uint64_t *red_mean)
{
    uint64_t si = 0, sr = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        si += (uint64_t)ir[i];
        sr += (uint64_t)red[i];
    }
    *ir_mean  = si / WINDOW_SIZE;
    *red_mean = sr / WINDOW_SIZE;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        ir[i]  -= (int32_t)(*ir_mean);
        red[i] -= (int32_t)(*red_mean);
    }
}

/*
 * Remove linear trend (baseline drift) via least-squares regression.
 * Σx = dt·N·(N-1)/2 ; for N=128, dt=0.04 → 325.12
 */
static void detrend(int32_t *buf)
{
    const double sx  = (SAMPLE_PERIOD_MS / 1000.0) *
                       (WINDOW_SIZE - 1.0) * WINDOW_SIZE / 2.0;
    int64_t      sy  = sum_y(buf);
    double       sx2 = sum_t2();
    double       sxy = sum_yt(buf);

    double a = (sxy - sx * (double)sy / WINDOW_SIZE) /
               (sx2 - sx * sx / WINDOW_SIZE);
    double b = ((double)sy / WINDOW_SIZE) - a * (sx / WINDOW_SIZE);

    double t = 0.0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        buf[i] = (int32_t)((buf[i] - a * t) - b);
        t += SAMPLE_PERIOD_MS / 1000.0;
    }
}

/* Autocorrelation at a given lag, normalised by N. */
static double acf(int32_t *data, int lag)
{
    double s = 0.0;
    for (int i = 0; i < WINDOW_SIZE - lag; i++)
        s += (double)data[i] * data[i + lag];
    return s / WINDOW_SIZE;
}

/* RMS of the AC signal. */
static double rms(int32_t *data)
{
    int64_t sq = 0;
    for (int i = 0; i < WINDOW_SIZE; i++)
        sq += (int64_t)data[i] * data[i];
    return sqrt((double)sq / WINDOW_SIZE);
}

/* Pearson correlation between RED and IR (signal quality indicator). */
static double pearson(int32_t *red, int32_t *ir)
{
    double sx = 0, sy = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) { sx += red[i]; sy += ir[i]; }
    double mx = sx / WINDOW_SIZE, my = sy / WINDOW_SIZE;
    double cov = 0, vx = 0, vy = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        double dx = red[i] - mx, dy = ir[i] - my;
        cov += dx * dy; vx += dx * dx; vy += dy * dy;
    }
    if (vx == 0.0 || vy == 0.0) return 0.0;
    return (cov / WINDOW_SIZE) / (sqrt(vx / WINDOW_SIZE) * sqrt(vy / WINDOW_SIZE));
}

/*
 * Heart rate via normalised autocorrelation.
 * lag > 14 avoids the 2nd harmonic (lag=11 → ~136 bpm while true HR ~68 bpm).
 * Returns BPM or -1 when no valid peak is found.
 */
static int calc_heart_rate(int32_t *ir, double *r0_out)
{
    double r0 = acf(ir, 0);
    *r0_out = r0;
    if (r0 == 0.0) return -1;

    double best = 0.0;
    int    best_lag = 0;

    for (int lag = 1; lag < 125; lag++) {
        double n = acf(ir, lag) / r0;
        s_acf[lag] = n;
        if (lag > 14 && n > MIN_ACF_RATIO) {
            if (n > best) {
                best     = n;
                best_lag = lag;
            } else if (best_lag > 0) {
                /* Past the peak — convert lag to BPM */
                return (int)(60.0 / (best_lag * (SAMPLE_PERIOD_MS / 1000.0)));
            }
        }
    }

    if (best_lag > 0)
        return (int)(60.0 / (best_lag * (SAMPLE_PERIOD_MS / 1000.0)));
    return -1;
}

/*
 * SpO2 via AC/DC ratio (Maxim empirical formula: SpO2 = -45.06·R² + 30.354·R + 94.845).
 *
 * This clone sensor has an inverted AC/DC relationship relative to a genuine
 * MAX30102 (IR shows lower relative AC than RED, opposite to oxyHb absorption).
 * R is therefore built as (IR_AC/IR_DC) / (RED_AC/RED_DC) instead of the usual inverse.
 */
static double calc_spo2(int32_t *ir, int32_t *red,
                        uint64_t ir_mean, uint64_t red_mean)
{
    double ir_rms  = rms(ir);
    double red_rms = rms(red);
    if (ir_mean == 0 || red_mean == 0 || red_rms == 0.0) return -1.0;

    double R = (ir_rms / (double)ir_mean) / (red_rms / (double)red_mean);
    return (-45.06 * R * R) + (30.354 * R) + 94.845;
}

/* =====================================================
 * FreeRTOS task
 * ===================================================== */

void max30102_task(void *pvParameters)
{
    uint32_t raw_red, raw_ir;

    while (1)
    {
        /* --- Collect one analysis window --- */
        for (int i = 0; i < WINDOW_SIZE; i++) {
            if (max30102_read_sample(&raw_red, &raw_ir) == ESP_OK) {
                s_red[i] = (int32_t)raw_red;
                s_ir[i]  = (int32_t)raw_ir;
            } else {
                s_red[i] = 0;
                s_ir[i]  = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
        }

        /* --- Finger detection via IR DC level --- */
        uint64_t ir_sum = 0;
        for (int i = 0; i < WINDOW_SIZE; i++) ir_sum += (uint64_t)s_ir[i];
        if (ir_sum / WINDOW_SIZE < MIN_IR_DC) {
            ESP_LOGI(TAG, "No finger (IR DC=%llu)", (unsigned long long)(ir_sum / WINDOW_SIZE));
            g_max_data.hr_valid   = false;
            g_max_data.spo2_valid = false;
            continue;
        }

        /* --- Remove DC (save means for SpO2) --- */
        uint64_t ir_mean = 0, red_mean = 0;
        remove_dc(s_ir, s_red, &ir_mean, &red_mean);

        /* --- Remove baseline drift --- */
        detrend(s_ir);
        detrend(s_red);

        /* --- Signal quality --- */
        double corr = pearson(s_red, s_ir);

        /* --- Heart rate --- */
        double r0 = 0.0;
        int hr = calc_heart_rate(s_ir, &r0);

        /* --- SpO2 ratio diagnostic --- */
        double ir_rms  = rms(s_ir);
        double red_rms = rms(s_red);
        double R_ratio = 0.0;
        if (ir_mean > 0 && red_mean > 0 && ir_rms > 0.0)
            R_ratio = (red_rms / (double)red_mean) / (ir_rms / (double)ir_mean);

        /* SpO2 only when signal is strong and R is physiologically plausible */
        double spo2 = -1.0;
        if (r0 >= 500.0 && R_ratio >= 1.20 && corr >= 0.85)
            spo2 = calc_spo2(s_ir, s_red, ir_mean, red_mean);

        /* --- Update global struct --- */
        g_max_data.hr_valid       = (hr > 30 && hr < 220);
        g_max_data.heart_rate_bpm = g_max_data.hr_valid   ? (float)hr   : 0.0f;
        g_max_data.spo2_valid     = (spo2 >= 70.0 && spo2 <= 100.0);
        g_max_data.spo2_percent   = g_max_data.spo2_valid ? (float)spo2 : 0.0f;

        ESP_LOGI(TAG,
                 "HR=%s(%.0f bpm) SpO2=%s(%.1f%%) R=%.3f corr=%.3f r0=%.0f",
                 g_max_data.hr_valid   ? "OK"  : "N/A", (double)g_max_data.heart_rate_bpm,
                 g_max_data.spo2_valid ? "OK"  : "N/A", (double)g_max_data.spo2_percent,
                 R_ratio, corr, r0);
                 
    }
}

/* =====================================================
 * Public API
 * ===================================================== */

void max30102_task_init(void)
{
    max30102_init();
    vTaskDelay(pdMS_TO_TICKS(100));

    xTaskCreate(
        max30102_task,
        "MAX30102 Task",
        8192,
        NULL,
        5,
        &s_task_handle
    );

    max30102_task_suspend();
}

void max30102_task_suspend(void)
{
    if (s_task_handle)
        vTaskSuspend(s_task_handle);
}

void max30102_task_resume(void)
{
    if (s_task_handle)
        vTaskResume(s_task_handle);
}
