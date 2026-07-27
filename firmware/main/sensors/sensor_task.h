// Probe-at-boot, absent-tolerant sensor sampling task.
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t sensor_task_start(void);  // returns ESP_OK even when sensor absent
bool      sensor_present(void);     // consumed by discovery

#ifdef __cplusplus
}
#endif
