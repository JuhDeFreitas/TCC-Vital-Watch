#include <time.h>

#include "esp_sntp.h"
#include "esp_log.h"

/**
 * @brief Initialize SNTP client and start time synchronization.
 */
void time_sync_init(void)
{
    if (esp_sntp_enabled()) {
        return;
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

/**
 * @brief Check whether the system time has been synchronized.
 *
 * @return true  Time is synchronized.
 * @return false Time is not synchronized.
 */
bool time_is_synced(void)
{
    time_t now;

    /* Read current system time */
    time(&now);

    /* Validate if a reasonable Unix timestamp is available */
    return (now > 1700000000);
}