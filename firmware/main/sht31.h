// SHT31 implementation of sensor_driver over the v6 i2c_master API.
#pragma once

#include <stdint.h>

#include "sensor_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

sensor_driver_t *sht31_get_driver(uint8_t sda_gpio, uint8_t scl_gpio);

#ifdef __cplusplus
}
#endif
