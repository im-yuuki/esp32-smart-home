// WiFi STA with esp_timer-driven exponential backoff (1 s .. 60 s cap).
// Posts APP_EVENT_WIFI_GOT_IP / APP_EVENT_WIFI_LOST on the default loop.
// Starts SNTP once after the first GOT_IP.
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t   wifi_manager_start(void);
bool        wifi_manager_is_connected(void);
const char *wifi_manager_get_ip_str(void);  // "192.168.1.57", cached from ip_event_got_ip_t

#ifdef __cplusplus
}
#endif
