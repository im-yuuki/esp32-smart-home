// NVS-backed node configuration.
//
// Namespaces:
//   shc_cfg   -- configuration, seeded ONCE from Kconfig defaults when the
//                schema-version key `cfg_ver` is absent (first boot / after
//                erase-flash). NVS is the source of truth afterwards.
//   shc_state -- relay states, written on every relay change, read by the
//                power-on restore path. Separate namespace so a future
//                "factory reset config" won't clobber relay state.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POWERON_RESTORE = 0,
    POWERON_OFF     = 1,
    POWERON_ON      = 2,
} poweron_behavior_t;

typedef struct {
    char     wifi_ssid[33], wifi_pass[65];
    char     mqtt_uri[128], mqtt_user[33], mqtt_pass[65];
    char     room[33];                       // slug, e.g. "phong-khach"
    uint8_t  relay_count;                    // fixed 2 in Phase 1
    uint8_t  relay_gpio[2];                  // default {4, 5}
    char     relay_name[2][33];              // "Den tran", "Den ban"
    uint8_t  relay_active_level;             // 1 = active-high (default), 0 = active-low
    poweron_behavior_t poweron;              // default POWERON_RESTORE
    uint8_t  button_gpio[2];                 // default {6, 7}
    uint8_t  i2c_sda, i2c_scl;               // default {8, 9}
    uint32_t sensor_interval_s;              // default 30
    char     node_id[16];                    // runtime: "esp32s3-a1b2c3"
} app_config_t;

esp_err_t           app_config_init(void);
const app_config_t *app_config_get(void);
esp_err_t           app_config_save_relay_state(uint8_t ch, bool on);   // ch is 1-based
esp_err_t           app_config_load_relay_state(uint8_t ch, bool *on);  // ESP_ERR_NVS_NOT_FOUND if never saved

#ifdef __cplusplus
}
#endif
