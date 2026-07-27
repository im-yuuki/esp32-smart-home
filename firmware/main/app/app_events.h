// Application-internal events posted on the default esp_event loop.
// Producers: wifi_manager (WIFI_*, PORTAL_START_REQ on unprovisioned boot and
// 180 s no-IP downtime), boot_button (PORTAL_START_REQ on 5 s hold), portal
// (PORTAL_STOP_REQ from its grace timer, PORTAL_STARTED/STOPPED info).
// Consumers: mqtt_mgr (WIFI_GOT_IP), portal (PORTAL_*_REQ, WIFI_*).
#pragma once

#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(APP_EVENT);

typedef enum {
    PORTAL_REASON_UNPROVISIONED = 0,  // no usable STA credentials at boot
    PORTAL_REASON_STA_DOWN      = 1,  // 180 s cumulative no-IP downtime
    PORTAL_REASON_MANUAL        = 2,  // BOOT button held 5 s
} portal_reason_t;

typedef enum {
    APP_EVENT_WIFI_GOT_IP,       // STA got (or re-got) an IPv4 address
    APP_EVENT_WIFI_LOST,         // STA disconnected from the AP
    APP_EVENT_PORTAL_START_REQ,  // data: int32_t (portal_reason_t)
    APP_EVENT_PORTAL_STOP_REQ,   // no data (posted by the portal grace timer)
    APP_EVENT_PORTAL_STARTED,    // data: int32_t reason -- informational (log/GD3)
    APP_EVENT_PORTAL_STOPPED,    // no data -- informational
} app_event_id_t;

#ifdef __cplusplus
}
#endif
