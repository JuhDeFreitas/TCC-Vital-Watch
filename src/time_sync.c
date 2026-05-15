#include <time.h>

#include "esp_sntp.h"
#include "esp_log.h"

static const char *TAG = "SNTP";

void time_sync_init(void)
{
    ESP_LOGI(TAG, "Inicializando SNTP");

    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    sntp_setservername(0, "pool.ntp.org");

    sntp_init();
}

bool time_is_synced(void)
{
    time_t now;

    time(&now);

    return (now > 1700000000);
}