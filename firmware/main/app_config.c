#include "app_config.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char *TAG = "app_config";

#define NS_CFG   "shc_cfg"
#define NS_STATE "shc_state"

#define CFG_SCHEMA_VER 1

#if defined(CONFIG_SHC_POWERON_OFF)
#define POWERON_DEFAULT POWERON_OFF
#elif defined(CONFIG_SHC_POWERON_ON)
#define POWERON_DEFAULT POWERON_ON
#else
#define POWERON_DEFAULT POWERON_RESTORE
#endif

#ifdef CONFIG_SHC_RELAY_ACTIVE_HIGH
#define RELAY_ACTIVE_DEFAULT 1
#else
#define RELAY_ACTIVE_DEFAULT 0
#endif

static app_config_t s_cfg;

// Bounded copy that can never trip -Wformat-truncation (unlike snprintf).
static void copy_str(char *dst, size_t cap, const char *src)
{
    size_t n = strnlen(src, cap - 1);
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void set_compile_time_defaults(void)
{
    memset(&s_cfg, 0, sizeof s_cfg);
    copy_str(s_cfg.wifi_ssid, sizeof s_cfg.wifi_ssid, CONFIG_SHC_WIFI_SSID);
    copy_str(s_cfg.wifi_pass, sizeof s_cfg.wifi_pass, CONFIG_SHC_WIFI_PASS);
    copy_str(s_cfg.mqtt_uri, sizeof s_cfg.mqtt_uri, CONFIG_SHC_MQTT_URI);
    copy_str(s_cfg.mqtt_user, sizeof s_cfg.mqtt_user, CONFIG_SHC_MQTT_USER);
    copy_str(s_cfg.mqtt_pass, sizeof s_cfg.mqtt_pass, CONFIG_SHC_MQTT_PASS);
    copy_str(s_cfg.room, sizeof s_cfg.room, CONFIG_SHC_ROOM);
    s_cfg.relay_count = 2;
    s_cfg.relay_gpio[0] = CONFIG_SHC_RELAY1_GPIO;
    s_cfg.relay_gpio[1] = CONFIG_SHC_RELAY2_GPIO;
    copy_str(s_cfg.relay_name[0], sizeof s_cfg.relay_name[0], CONFIG_SHC_RELAY1_NAME);
    copy_str(s_cfg.relay_name[1], sizeof s_cfg.relay_name[1], CONFIG_SHC_RELAY2_NAME);
    s_cfg.relay_active_level = RELAY_ACTIVE_DEFAULT;
    s_cfg.poweron = POWERON_DEFAULT;
    s_cfg.button_gpio[0] = CONFIG_SHC_BUTTON1_GPIO;
    s_cfg.button_gpio[1] = CONFIG_SHC_BUTTON2_GPIO;
    s_cfg.i2c_sda = CONFIG_SHC_I2C_SDA_GPIO;
    s_cfg.i2c_scl = CONFIG_SHC_I2C_SCL_GPIO;
    s_cfg.sensor_interval_s = CONFIG_SHC_SENSOR_INTERVAL_S;
}

static esp_err_t seed_from_kconfig(nvs_handle_t h)
{
    ESP_LOGI(TAG, "first boot: seeding config from Kconfig defaults");
    esp_err_t err;
    if ((err = nvs_set_str(h, "wifi_ssid", CONFIG_SHC_WIFI_SSID)) != ESP_OK) return err;
    if ((err = nvs_set_str(h, "wifi_pass", CONFIG_SHC_WIFI_PASS)) != ESP_OK) return err;
    if ((err = nvs_set_str(h, "mqtt_uri", CONFIG_SHC_MQTT_URI)) != ESP_OK) return err;
    if ((err = nvs_set_str(h, "mqtt_user", CONFIG_SHC_MQTT_USER)) != ESP_OK) return err;
    if ((err = nvs_set_str(h, "mqtt_pass", CONFIG_SHC_MQTT_PASS)) != ESP_OK) return err;
    if ((err = nvs_set_str(h, "room", CONFIG_SHC_ROOM)) != ESP_OK) return err;
    if ((err = nvs_set_u8(h, "relay_cnt", 2)) != ESP_OK) return err;
    if ((err = nvs_set_u8(h, "r1_gpio", CONFIG_SHC_RELAY1_GPIO)) != ESP_OK) return err;
    if ((err = nvs_set_u8(h, "r2_gpio", CONFIG_SHC_RELAY2_GPIO)) != ESP_OK) return err;
    if ((err = nvs_set_str(h, "r1_name", CONFIG_SHC_RELAY1_NAME)) != ESP_OK) return err;
    if ((err = nvs_set_str(h, "r2_name", CONFIG_SHC_RELAY2_NAME)) != ESP_OK) return err;
    if ((err = nvs_set_u8(h, "relay_act", RELAY_ACTIVE_DEFAULT)) != ESP_OK) return err;
    if ((err = nvs_set_u8(h, "poweron", (uint8_t)POWERON_DEFAULT)) != ESP_OK) return err;
    if ((err = nvs_set_u8(h, "btn1_gpio", CONFIG_SHC_BUTTON1_GPIO)) != ESP_OK) return err;
    if ((err = nvs_set_u8(h, "btn2_gpio", CONFIG_SHC_BUTTON2_GPIO)) != ESP_OK) return err;
    if ((err = nvs_set_u8(h, "i2c_sda", CONFIG_SHC_I2C_SDA_GPIO)) != ESP_OK) return err;
    if ((err = nvs_set_u8(h, "i2c_scl", CONFIG_SHC_I2C_SCL_GPIO)) != ESP_OK) return err;
    if ((err = nvs_set_u32(h, "sens_int_s", CONFIG_SHC_SENSOR_INTERVAL_S)) != ESP_OK) return err;
    // cfg_ver last: its presence marks the seed as complete.
    if ((err = nvs_set_u8(h, "cfg_ver", CFG_SCHEMA_VER)) != ESP_OK) return err;
    return nvs_commit(h);
}

// Loaders tolerate a missing key (keep the compiled-in default, warn) so a
// partially-written config can't brick the node.
static void load_str(nvs_handle_t h, const char *key, char *dst, size_t cap)
{
    size_t len = cap;
    esp_err_t err = nvs_get_str(h, key, dst, &len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cfg key '%s': %s -> keeping compiled default", key, esp_err_to_name(err));
    }
}

static void load_u8(nvs_handle_t h, const char *key, uint8_t *dst)
{
    esp_err_t err = nvs_get_u8(h, key, dst);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cfg key '%s': %s -> keeping compiled default", key, esp_err_to_name(err));
    }
}

static void load_u32(nvs_handle_t h, const char *key, uint32_t *dst)
{
    esp_err_t err = nvs_get_u32(h, key, dst);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "cfg key '%s': %s -> keeping compiled default", key, esp_err_to_name(err));
    }
}

esp_err_t app_config_init(void)
{
    set_compile_time_defaults();

    nvs_handle_t h;
    esp_err_t err = nvs_open(NS_CFG, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(%s) failed: %s", NS_CFG, esp_err_to_name(err));
        return err;
    }

    uint8_t ver = 0;
    err = nvs_get_u8(h, "cfg_ver", &ver);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = seed_from_kconfig(h);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "seeding failed: %s", esp_err_to_name(err));
            nvs_close(h);
            return err;
        }
        ver = CFG_SCHEMA_VER;
    } else if (err != ESP_OK) {
        nvs_close(h);
        return err;
    } else {
        ESP_LOGI(TAG, "config loaded (ver %u)", (unsigned)ver);
    }

    load_str(h, "wifi_ssid", s_cfg.wifi_ssid, sizeof s_cfg.wifi_ssid);
    load_str(h, "wifi_pass", s_cfg.wifi_pass, sizeof s_cfg.wifi_pass);
    load_str(h, "mqtt_uri", s_cfg.mqtt_uri, sizeof s_cfg.mqtt_uri);
    load_str(h, "mqtt_user", s_cfg.mqtt_user, sizeof s_cfg.mqtt_user);
    load_str(h, "mqtt_pass", s_cfg.mqtt_pass, sizeof s_cfg.mqtt_pass);
    load_str(h, "room", s_cfg.room, sizeof s_cfg.room);
    load_u8(h, "relay_cnt", &s_cfg.relay_count);
    load_u8(h, "r1_gpio", &s_cfg.relay_gpio[0]);
    load_u8(h, "r2_gpio", &s_cfg.relay_gpio[1]);
    load_str(h, "r1_name", s_cfg.relay_name[0], sizeof s_cfg.relay_name[0]);
    load_str(h, "r2_name", s_cfg.relay_name[1], sizeof s_cfg.relay_name[1]);
    load_u8(h, "relay_act", &s_cfg.relay_active_level);
    uint8_t poweron = (uint8_t)POWERON_DEFAULT;
    load_u8(h, "poweron", &poweron);
    s_cfg.poweron = (poweron_behavior_t)poweron;
    load_u8(h, "btn1_gpio", &s_cfg.button_gpio[0]);
    load_u8(h, "btn2_gpio", &s_cfg.button_gpio[1]);
    load_u8(h, "i2c_sda", &s_cfg.i2c_sda);
    load_u8(h, "i2c_scl", &s_cfg.i2c_scl);
    load_u32(h, "sens_int_s", &s_cfg.sensor_interval_s);
    nvs_close(h);

    // Defensive clamps (arrays are sized for 2 channels).
    if (s_cfg.relay_count > 2) {
        ESP_LOGW(TAG, "relay_cnt %u clamped to 2", (unsigned)s_cfg.relay_count);
        s_cfg.relay_count = 2;
    }
    if (s_cfg.sensor_interval_s < 5) {
        s_cfg.sensor_interval_s = 5;
    }

    // node_id: last 3 STA-MAC bytes, lowercase hex. Works before esp_wifi_init.
    uint8_t mac[6] = {0};
    err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_read_mac failed: %s", esp_err_to_name(err));
        return err;
    }
    snprintf(s_cfg.node_id, sizeof s_cfg.node_id, "esp32s3-%02x%02x%02x", mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "node_id=%s room=%s mqtt=%s", s_cfg.node_id, s_cfg.room, s_cfg.mqtt_uri);
    ESP_LOGI(TAG, "relays GPIO %u/%u (%s), buttons GPIO %u/%u, I2C SDA=%u SCL=%u, poweron=%d",
             (unsigned)s_cfg.relay_gpio[0], (unsigned)s_cfg.relay_gpio[1],
             s_cfg.relay_active_level ? "active-high" : "active-low",
             (unsigned)s_cfg.button_gpio[0], (unsigned)s_cfg.button_gpio[1],
             (unsigned)s_cfg.i2c_sda, (unsigned)s_cfg.i2c_scl, (int)s_cfg.poweron);
    return ESP_OK;
}

const app_config_t *app_config_get(void)
{
    return &s_cfg;
}

esp_err_t app_config_save_relay_state(uint8_t ch, bool on)
{
    if (ch < 1 || ch > s_cfg.relay_count) {
        return ESP_ERR_INVALID_ARG;
    }
    char key[16];
    snprintf(key, sizeof key, "r%u_state", (unsigned)ch);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NS_STATE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u8(h, key, on ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t app_config_load_relay_state(uint8_t ch, bool *on)
{
    if (ch < 1 || ch > s_cfg.relay_count || on == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    char key[16];
    snprintf(key, sizeof key, "r%u_state", (unsigned)ch);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NS_STATE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return err;  // ESP_ERR_NVS_NOT_FOUND when namespace never written
    }
    uint8_t v = 0;
    err = nvs_get_u8(h, key, &v);
    nvs_close(h);
    if (err == ESP_OK) {
        *on = (v != 0);
    }
    return err;
}
