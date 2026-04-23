#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

void wifi_init(void);
void wifi_start(void);
bool wifi_is_connected(void);

#endif