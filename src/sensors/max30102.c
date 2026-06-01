#include "sensors/max30102.h"
#include "sensors/max30102_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_err.h"

#include <math.h>

#include "mqtt/mqtt.h"
#include "mqtt/payload.h"
#include "device_state.h"
#include "device_info.h"

//extern QueueHandle_t sensor_queue;

static const char *TAG = "MAX30102_APP";

max30102_data_t g_max_data; 

/* Funções privadas auxiliares ====================================================== */

static float mean(const uint32_t *x,int n){
    float s=0;
    for(int i=0;i<n;i++) s+=x[i];
    return s/n;
}

static float clamp(float v,float a,float b){
    if(v<a) return a;
    if(v>b) return b;
    return v;
}

static esp_err_t max30102_setup(void)
{
    esp_err_t err = max30102_init();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao inicializar MAX30102: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "MAX30102 inicializado");
    return ESP_OK;
}

static esp_err_t max30102_read_window(uint32_t *red, uint32_t *ir)
{
    esp_err_t err = max30102_collect_window(
        red,
        ir,
        MAX30102_BUFFER_SIZE
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro na coleta de amostras");
    }

    return err;
}

static int max30102_compute_metrics(const uint32_t *red,
                             const uint32_t *ir,
                             int len,
                             float fs,
                             max30102_data_t *out)
{
    if(len < 50) return -1;

    float dc_red = mean(red,len);
    float dc_ir  = mean(ir,len);

    float max_r=0,min_r=1e9;
    float max_i=0,min_i=1e9;

    for(int i=0;i<len;i++){
        if(red[i]>max_r) max_r=red[i];
        if(red[i]<min_r) min_r=red[i];
        if(ir[i]>max_i) max_i=ir[i];
        if(ir[i]<min_i) min_i=ir[i];
    }

    float ac_red = max_r - min_r;
    float ac_ir  = max_i - min_i;

    float R = (ac_red/dc_red)/(ac_ir/dc_ir);

    out->spo2_percent = clamp(104.0f - 17.0f*R, 0, 100);
    out->spo2_valid = true;

    /* Placeholder HR (se quiser depois colocamos seu algoritmo completo) */
    out->heart_rate_bpm = 75;
    out->hr_valid = true;

    return 0;
}

static esp_err_t max30102_process(uint32_t *red,
                                  uint32_t *ir,
                                  max30102_data_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int ret = max30102_compute_metrics(
        red,
        ir,
        MAX30102_BUFFER_SIZE,
        MAX30102_SAMPLE_RATE_HZ,
        out
    );

    if (ret != 0) {
        ESP_LOGE(TAG, "Erro no cálculo de métricas");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void max30102_publish(const max30102_data_t *metrics)
{
    char payload[256];

    if ( build_max30102_payload(metrics, payload, sizeof(payload)) )
    {
        mqtt_publish_message(TOPIC_VITALS, payload);
    }
    else
    {
        ESP_LOGE(TAG, "Erro ao criar payload JSON");
    }
}


/* Função principal (Task) ======================================================== */

void max30102_task(void *pvParameters)
{
    //QueueHandle_t queue = (QueueHandle_t) pvParameters;

    static uint32_t red[MAX30102_BUFFER_SIZE];
    static uint32_t ir[MAX30102_BUFFER_SIZE];

    if (max30102_setup() != ESP_OK) {
        vTaskDelete(NULL);
    }

    while (1) {

        /* Aquisição dos dados */
        if (max30102_read_window(red, ir) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        /* Processamento das metricas */
        max30102_data_t metrics;

        if (max30102_process(red, ir, &metrics) != ESP_OK) {
            continue;
        }

        /* Log dos valores obtidos*/
        //ESP_LOGI(TAG,"HR=%.1f (%d) | SpO2=%.1f (%d)",
        //     metrics.heart_rate_bpm,
        //     metrics.hr_valid,
        //     metrics.spo2_percent,
        //     metrics.spo2_valid);

        /* Publicação do valores */
        max30102_publish(&metrics);
        g_max_data = metrics;
        //xQueueOverwrite(queue, &metrics);

        vTaskDelay(pdMS_TO_TICKS(get_sampling_interval()));
    }
}