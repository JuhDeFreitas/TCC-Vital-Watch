#ifndef TIME_SYNC_H
#define TIME_SYNC_H


/**
 * @brief Initialize SNTP client and start time synchronization.
 */
void time_sync_init(void);


/**
 * @brief Check whether the system time has been synchronized.
 *
 * @return true  Time is synchronized.
 * @return false Time is not synchronized.
 */
bool time_is_synced(void);

#endif