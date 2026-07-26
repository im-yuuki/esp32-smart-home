// Physical buttons: GPIO ISR -> 50 ms one-shot esp_timer (debounce) ->
// callback resamples the level -> TOGGLE command into the relay queue.
// Works end-to-end with WiFi/MQTT down.
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t button_handler_init(void);

#ifdef __cplusplus
}
#endif
