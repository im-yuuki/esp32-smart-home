// Generic sensor driver vtable -- the seam for a future DHT22.
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sensor_driver {
    const char *model;                                    // "SHT31"
    const char *kind;                                     // "temperature_humidity"
    esp_err_t (*init)(struct sensor_driver *drv);         // bus/dev setup; may allocate ctx
    esp_err_t (*probe)(struct sensor_driver *drv);        // ESP_OK | ESP_ERR_NOT_FOUND | ESP_ERR_TIMEOUT
    esp_err_t (*read)(struct sensor_driver *drv, float *temp_c, float *rh);
    void (*deinit)(struct sensor_driver *drv);            // release bus/device (sensor-absent path)
    void *ctx;
} sensor_driver_t;

#ifdef __cplusplus
}
#endif
