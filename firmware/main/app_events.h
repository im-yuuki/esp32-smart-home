// Application-internal events posted on the default esp_event loop.
// Producer: wifi_manager. Consumer: mqtt_mgr.
#pragma once

#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(APP_EVENT);

typedef enum {
    APP_EVENT_WIFI_GOT_IP,  // STA got (or re-got) an IPv4 address
    APP_EVENT_WIFI_LOST,    // STA disconnected from the AP
} app_event_id_t;

#ifdef __cplusplus
}
#endif
