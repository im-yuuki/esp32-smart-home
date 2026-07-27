// BOOT button (GPIO0) hold detector: ISR -> 50 ms debounce esp_timer -> per-
// duration hold timers. Holding 5 s posts APP_EVENT_PORTAL_START_REQ (manual
// portal trigger); the hold table is structured so GD3 can add a longer
// factory-reset hold without touching the detection logic.
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t boot_button_init(void);

#ifdef __cplusplus
}
#endif
