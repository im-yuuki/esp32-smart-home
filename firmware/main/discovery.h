// cJSON discovery payload builder. Called only from MQTT_EVENT_CONNECTED.
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t discovery_publish(void);

#ifdef __cplusplus
}
#endif
