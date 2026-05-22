#include <time.h>

#include "cJSON.h"
#include "mqtt/payload.h"
#include "sensors/max30102.h"

bool build_max30102_payload(const max30102_data_t *metrics, char *buffer, size_t buffer_size)
{
    if (metrics == NULL || buffer == NULL){
        return false;
    }

    cJSON *root = cJSON_CreateObject();

    if (root == NULL){
        return false;
    }

    time_t now;

    time(&now);

    cJSON_AddNumberToObject(root, "timestamp", (double)now);
    cJSON_AddNumberToObject(root, "heart_rate_bpm", metrics->heart_rate_bpm);
    cJSON_AddNumberToObject(root, "spo2_percent", metrics->spo2_percent);

    bool success = cJSON_PrintPreallocated(root, buffer, buffer_size, false);

    cJSON_Delete(root);

    return success;
}


bool build_alert_payload(const char *severity, char *buffer, size_t buffer_size)
{
    if (severity == NULL || buffer == NULL){
        return false;
    }

    cJSON *root = cJSON_CreateObject();

    if (root == NULL){
        return false;
    }

    time_t now;
    time(&now);

    cJSON_AddStringToObject(root, "type", "fall");
    cJSON_AddStringToObject(root, "severity", severity);
    cJSON_AddNumberToObject(root, "timestamp", (double)now);

    bool success = cJSON_PrintPreallocated(root, buffer, buffer_size, false);

    cJSON_Delete(root);

    return success;
}
