#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

/** \brief WiFi configuration parameters */
#define WIFI_SSID     "Julia"
#define WIFI_PASSWORD "13020011"

//#define WIFI_SSID     "WLL-Inatel"
//#define WIFI_PASSWORD "Inatelsemfio"

/* Function prototypes */
void wifi_init(void);
void wifi_start(void);
bool wifi_is_connected(void);

// wifi_config() - função para configurar o WiFi

#endif