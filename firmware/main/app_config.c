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

#define CFG_SCHEMA_VER 2

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
    // v2 (portal): no backup AP, DHCP, empty static fields (memset above),
    // admin password from Kconfig.
    s_cfg.ip_mode = 0;
    copy_str(s_cfg.admin_pass, sizeof s_cfg.admin_pass, CONFIG_SHC_ADMIN_PASS);
}

// Seeding is IDEMPOTENT: a key that already exists is left exactly as it is.
//
// This is not defensive decoration, it is what keeps a retried seed/migration
// from silently downgrading a configured node. nvs_commit is a documented no-op
// in this IDF -- every nvs_set_* is durable the moment it returns -- so a
// seed/migration that dies partway through has ALREADY written whatever it got
// to. cfg_ver is written last and would still be missing/1, so the whole
// sequence re-runs on the next boot. In between, the node boots fine and the
// user configures it through the portal. Unconditional writes would then wipe
// that work on the next boot, and for admin_pass that means reverting to the
// compiled-in, well-known CONFIG_SHC_ADMIN_PASS: a loud boot loop traded for a
// silent auth downgrade. First boot is unaffected -- no key exists yet, so
// every value below is written.
//
// Only ESP_ERR_NVS_NOT_FOUND means "absent". nvs_get_* forwards storage-layer
// errors verbatim (nvs_api.cpp: nvs_get_str_or_blob and friends return whatever
// the storage layer gave them), so treating every non-ESP_OK as absent would
// turn a transient flash read error on admin_pass into exactly the silent auth
// downgrade this whole block exists to prevent -- one NVS error deeper.
// "Couldn't tell" is therefore propagated as a migration/seed failure, which
// the callers already handle non-fatally and retry on the next boot.
static esp_err_t set_str_if_absent(nvs_handle_t h, const char *key, const char *val)
{
    size_t len = 0;
    esp_err_t e = nvs_get_str(h, key, NULL, &len);
    if (e == ESP_OK) {
        return ESP_OK;  // present -- never clobber a value the user may own
    }
    if (e != ESP_ERR_NVS_NOT_FOUND) {
        return e;
    }
    return nvs_set_str(h, key, val);
}

static esp_err_t set_u8_if_absent(nvs_handle_t h, const char *key, uint8_t val)
{
    uint8_t cur = 0;
    esp_err_t e = nvs_get_u8(h, key, &cur);
    if (e == ESP_OK) {
        return ESP_OK;
    }
    if (e != ESP_ERR_NVS_NOT_FOUND) {
        return e;
    }
    return nvs_set_u8(h, key, val);
}

static esp_err_t set_u32_if_absent(nvs_handle_t h, const char *key, uint32_t val)
{
    uint32_t cur = 0;
    esp_err_t e = nvs_get_u32(h, key, &cur);
    if (e == ESP_OK) {
        return ESP_OK;
    }
    if (e != ESP_ERR_NVS_NOT_FOUND) {
        return e;
    }
    return nvs_set_u32(h, key, val);
}

// The 8 v2 keys share their defaults between first-boot seeding and the
// v1 -> v2 migration: backup AP empty, DHCP, empty static-IP fields, Kconfig
// admin password. Absent keys only -- see set_str_if_absent.
static esp_err_t write_v2_defaults(nvs_handle_t h)
{
    esp_err_t err;
    if ((err = set_str_if_absent(h, "bak_ssid", "")) != ESP_OK) return err;
    if ((err = set_str_if_absent(h, "bak_pass", "")) != ESP_OK) return err;
    if ((err = set_u8_if_absent(h, "ip_mode", 0)) != ESP_OK) return err;
    if ((err = set_str_if_absent(h, "st_ip", "")) != ESP_OK) return err;
    if ((err = set_str_if_absent(h, "st_nm", "")) != ESP_OK) return err;
    if ((err = set_str_if_absent(h, "st_gw", "")) != ESP_OK) return err;
    if ((err = set_str_if_absent(h, "st_dns", "")) != ESP_OK) return err;
    return set_str_if_absent(h, "admin_pass", CONFIG_SHC_ADMIN_PASS);
}

// v1 -> v2: seed only the keys that do not exist yet, so v1 values survive and
// so does anything a portal user wrote after a previous attempt failed part-way
// (see set_str_if_absent). cfg_ver bumped last, then committed.
static esp_err_t migrate_v1_to_v2(nvs_handle_t h)
{
    ESP_LOGI(TAG, "migrating config v1 -> v2 (missing keys seeded with defaults)");
    esp_err_t err = write_v2_defaults(h);
    if (err != ESP_OK) return err;
    if ((err = nvs_set_u8(h, "cfg_ver", CFG_SCHEMA_VER)) != ESP_OK) return err;
    return nvs_commit(h);
}

static esp_err_t seed_from_kconfig(nvs_handle_t h)
{
    ESP_LOGI(TAG, "seeding missing config keys from Kconfig defaults");
    esp_err_t err;
    if ((err = set_str_if_absent(h, "wifi_ssid", CONFIG_SHC_WIFI_SSID)) != ESP_OK) return err;
    if ((err = set_str_if_absent(h, "wifi_pass", CONFIG_SHC_WIFI_PASS)) != ESP_OK) return err;
    if ((err = set_str_if_absent(h, "mqtt_uri", CONFIG_SHC_MQTT_URI)) != ESP_OK) return err;
    if ((err = set_str_if_absent(h, "mqtt_user", CONFIG_SHC_MQTT_USER)) != ESP_OK) return err;
    if ((err = set_str_if_absent(h, "mqtt_pass", CONFIG_SHC_MQTT_PASS)) != ESP_OK) return err;
    if ((err = set_str_if_absent(h, "room", CONFIG_SHC_ROOM)) != ESP_OK) return err;
    if ((err = set_u8_if_absent(h, "relay_cnt", 2)) != ESP_OK) return err;
    if ((err = set_u8_if_absent(h, "r1_gpio", CONFIG_SHC_RELAY1_GPIO)) != ESP_OK) return err;
    if ((err = set_u8_if_absent(h, "r2_gpio", CONFIG_SHC_RELAY2_GPIO)) != ESP_OK) return err;
    if ((err = set_str_if_absent(h, "r1_name", CONFIG_SHC_RELAY1_NAME)) != ESP_OK) return err;
    if ((err = set_str_if_absent(h, "r2_name", CONFIG_SHC_RELAY2_NAME)) != ESP_OK) return err;
    if ((err = set_u8_if_absent(h, "relay_act", RELAY_ACTIVE_DEFAULT)) != ESP_OK) return err;
    if ((err = set_u8_if_absent(h, "poweron", (uint8_t)POWERON_DEFAULT)) != ESP_OK) return err;
    if ((err = set_u8_if_absent(h, "btn1_gpio", CONFIG_SHC_BUTTON1_GPIO)) != ESP_OK) return err;
    if ((err = set_u8_if_absent(h, "btn2_gpio", CONFIG_SHC_BUTTON2_GPIO)) != ESP_OK) return err;
    if ((err = set_u8_if_absent(h, "i2c_sda", CONFIG_SHC_I2C_SDA_GPIO)) != ESP_OK) return err;
    if ((err = set_u8_if_absent(h, "i2c_scl", CONFIG_SHC_I2C_SCL_GPIO)) != ESP_OK) return err;
    if ((err = set_u32_if_absent(h, "sens_int_s", CONFIG_SHC_SENSOR_INTERVAL_S)) != ESP_OK) return err;
    if ((err = write_v2_defaults(h)) != ESP_OK) return err;
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
    if (err != ESP_OK) {
        // Absent cfg_ver = first boot. UNREADABLE cfg_ver is handled the same
        // way on purpose: returning the error here goes through ESP_ERROR_CHECK
        // in app_main -> panic -> reset -> retry forever, i.e. one NVS hiccup
        // bricks a node whose relays and buttons are perfectly fine. Seeding is
        // safe to run blind because every write is if-absent (see
        // set_str_if_absent): existing keys, including anything the user set
        // through the portal, are left exactly as they are.
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "cfg_ver unreadable: %s -- seeding missing keys and continuing",
                     esp_err_to_name(err));
        }
        err = seed_from_kconfig(h);
        if (err != ESP_OK) {
            // Non-fatal, for the same reason the v1 -> v2 failure below is:
            // app_main wraps app_config_init in ESP_ERROR_CHECK, so returning
            // an error panics -> resets -> retries forever, and a node whose
            // relays and buttons work perfectly would be bricked by one NVS
            // hiccup. The loaders below tolerate every missing key (compiled-in
            // default + warning), which is exactly what seeding would have
            // written. cfg_ver is written last, so the seed is simply retried
            // next boot -- and it is idempotent, so the retry cannot overwrite
            // anything the user configured in the meantime.
            ESP_LOGE(TAG, "seeding failed: %s -- running on compiled-in defaults "
                          "(retried on next boot)", esp_err_to_name(err));
        } else {
            ver = CFG_SCHEMA_VER;
        }
    } else if (ver == 1) {
        err = migrate_v1_to_v2(h);
        if (err != ESP_OK) {
            // Non-fatal on purpose. app_main wraps app_config_init in
            // ESP_ERROR_CHECK, so returning here panics -> resets -> retries
            // forever: one NVS hiccup during an upgrade would brick a node
            // whose relays are perfectly fine. Fall through to the loaders
            // instead -- they tolerate every missing key (compiled-in default
            // + warning), which for the 8 v2 keys is exactly what the
            // migration would have written. cfg_ver is bumped only after all
            // writes succeed, so the migration is simply retried next boot.
            ESP_LOGE(TAG, "v1 -> v2 migration failed: %s -- continuing with defaults for the new keys "
                          "(retried on next boot)", esp_err_to_name(err));
        } else {
            ver = CFG_SCHEMA_VER;
        }
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
    load_str(h, "bak_ssid", s_cfg.bak_ssid, sizeof s_cfg.bak_ssid);
    load_str(h, "bak_pass", s_cfg.bak_pass, sizeof s_cfg.bak_pass);
    load_u8(h, "ip_mode", &s_cfg.ip_mode);
    load_str(h, "st_ip", s_cfg.st_ip, sizeof s_cfg.st_ip);
    load_str(h, "st_nm", s_cfg.st_nm, sizeof s_cfg.st_nm);
    load_str(h, "st_gw", s_cfg.st_gw, sizeof s_cfg.st_gw);
    load_str(h, "st_dns", s_cfg.st_dns, sizeof s_cfg.st_dns);
    load_str(h, "admin_pass", s_cfg.admin_pass, sizeof s_cfg.admin_pass);
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
    // A failure here is NOT fatal: app_main wraps app_config_init in
    // ESP_ERROR_CHECK, so returning the error panics -> resets -> retries
    // forever. A node that cannot read its own MAC is broken, but it must still
    // switch the lights -- relays, buttons and the recovery portal do not need
    // an identity. mac[] is still all-zero here, so node_id falls back to the
    // obviously-wrong "esp32s3-000000", loudly logged.
    uint8_t mac[6] = {0};
    err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_read_mac failed: %s -- node_id falls back to esp32s3-000000; "
                      "MQTT topics and discovery will collide with any other node in this state",
                 esp_err_to_name(err));
    }
    snprintf(s_cfg.node_id, sizeof s_cfg.node_id, "esp32s3-%02x%02x%02x", mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "node_id=%s room=%s mqtt=%s", s_cfg.node_id, s_cfg.room, s_cfg.mqtt_uri);
    ESP_LOGI(TAG, "relays GPIO %u/%u (%s), buttons GPIO %u/%u, I2C SDA=%u SCL=%u, poweron=%d",
             (unsigned)s_cfg.relay_gpio[0], (unsigned)s_cfg.relay_gpio[1],
             s_cfg.relay_active_level ? "active-high" : "active-low",
             (unsigned)s_cfg.button_gpio[0], (unsigned)s_cfg.button_gpio[1],
             (unsigned)s_cfg.i2c_sda, (unsigned)s_cfg.i2c_scl, (int)s_cfg.poweron);
    // Never log passwords -- only whether the portal password is still default.
    ESP_LOGI(TAG, "ip_mode=%s backup_ap=%s admin_pass=%s",
             s_cfg.ip_mode == 1 ? "static" : "dhcp",
             s_cfg.bak_ssid[0] != '\0' ? "set" : "none",
             app_config_admin_pass_is_default() ? "DEFAULT (change via portal)" : "custom");
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

// key/value/mirror tuple for the portal setters below.
typedef struct {
    const char *key;
    const char *val;   // NULL = leave this key untouched
    char       *dst;   // s_cfg mirror
    size_t      cap;
} cfg_str_kv_t;

// Write every non-NULL value under its key in shc_cfg, commit, then mirror
// into s_cfg (mirror only after a successful commit so RAM never diverges
// from flash).
static esp_err_t save_strings(const cfg_str_kv_t *kv, size_t n)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS_CFG, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    for (size_t i = 0; err == ESP_OK && i < n; i++) {
        if (kv[i].val != NULL) {
            err = nvs_set_str(h, kv[i].key, kv[i].val);
        }
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err == ESP_OK) {
        for (size_t i = 0; i < n; i++) {
            if (kv[i].val != NULL) {
                copy_str(kv[i].dst, kv[i].cap, kv[i].val);
            }
        }
    }
    return err;
}

esp_err_t app_config_save_wifi(const char *ssid, const char *pass, const char *bak_ssid, const char *bak_pass)
{
    const cfg_str_kv_t kv[] = {
        { "wifi_ssid", ssid, s_cfg.wifi_ssid, sizeof s_cfg.wifi_ssid },
        { "wifi_pass", pass, s_cfg.wifi_pass, sizeof s_cfg.wifi_pass },
        { "bak_ssid", bak_ssid, s_cfg.bak_ssid, sizeof s_cfg.bak_ssid },
        { "bak_pass", bak_pass, s_cfg.bak_pass, sizeof s_cfg.bak_pass },
    };
    return save_strings(kv, sizeof kv / sizeof kv[0]);
}

esp_err_t app_config_save_ip(uint8_t ip_mode, const char *ip, const char *nm, const char *gw, const char *dns)
{
    const cfg_str_kv_t kv[] = {
        { "st_ip", ip, s_cfg.st_ip, sizeof s_cfg.st_ip },
        { "st_nm", nm, s_cfg.st_nm, sizeof s_cfg.st_nm },
        { "st_gw", gw, s_cfg.st_gw, sizeof s_cfg.st_gw },
        { "st_dns", dns, s_cfg.st_dns, sizeof s_cfg.st_dns },
    };
    const size_t n = sizeof kv / sizeof kv[0];

    nvs_handle_t h;
    esp_err_t err = nvs_open(NS_CFG, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u8(h, "ip_mode", ip_mode);
    for (size_t i = 0; err == ESP_OK && i < n; i++) {
        if (kv[i].val != NULL) {
            err = nvs_set_str(h, kv[i].key, kv[i].val);
        }
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err == ESP_OK) {
        s_cfg.ip_mode = ip_mode;
        for (size_t i = 0; i < n; i++) {
            if (kv[i].val != NULL) {
                copy_str(kv[i].dst, kv[i].cap, kv[i].val);
            }
        }
    }
    return err;
}

esp_err_t app_config_save_mqtt(const char *uri, const char *user, const char *pass)
{
    const cfg_str_kv_t kv[] = {
        { "mqtt_uri", uri, s_cfg.mqtt_uri, sizeof s_cfg.mqtt_uri },
        { "mqtt_user", user, s_cfg.mqtt_user, sizeof s_cfg.mqtt_user },
        { "mqtt_pass", pass, s_cfg.mqtt_pass, sizeof s_cfg.mqtt_pass },
    };
    return save_strings(kv, sizeof kv / sizeof kv[0]);
}

esp_err_t app_config_save_admin_pass(const char *pass)
{
    const cfg_str_kv_t kv[] = {
        { "admin_pass", pass, s_cfg.admin_pass, sizeof s_cfg.admin_pass },
    };
    return save_strings(kv, sizeof kv / sizeof kv[0]);
}

bool app_config_admin_pass_is_default(void)
{
    return strcmp(s_cfg.admin_pass, CONFIG_SHC_ADMIN_PASS) == 0;
}
