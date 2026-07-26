#include "wifi_manager.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "app_config.h"
#include "app_events.h"

static const char *TAG = "wifi";

#define BACKOFF_MIN_MS 1000u
#define BACKOFF_MAX_MS 60000u

static esp_timer_handle_t s_reconnect_timer;
static uint32_t s_backoff_ms = BACKOFF_MIN_MS;
static volatile bool s_connected;
static bool s_sntp_started;
static char s_ip_str[16] = "0.0.0.0";

// Bounded copy that can never trip -Wformat-truncation (unlike snprintf).
static void copy_bytes(uint8_t *dst, size_t cap, const char *src)
{
    size_t n = strnlen(src, cap - 1);
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void start_sntp_once(void)
{
    if (s_sntp_started) {
        return;
    }
    esp_sntp_config_t sc = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    sc.wait_for_sync = false;  // nobody blocks on sync; ts is best-effort
    esp_err_t err = esp_netif_sntp_init(&sc);
    if (err == ESP_OK) {
        s_sntp_started = true;
        ESP_LOGI(TAG, "SNTP started (pool.ntp.org)");
    } else {
        ESP_LOGW(TAG, "SNTP init failed: %s (ts stays epoch-relative)", esp_err_to_name(err));
    }
}

// esp_timer callback: retry the connection. Nothing else -- never blocks,
// never re-inits WiFi (a second esp_wifi_init returns ESP_ERR_INVALID_STATE).
static void reconnect_cb(void *arg)
{
    (void)arg;
    esp_wifi_connect();
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    if (id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = (const wifi_event_sta_disconnected_t *)data;
        s_connected = false;
        // Log the reason NUMERICALLY -- several WIFI_REASON_* enums were
        // removed in v6; do not switch on them.
        ESP_LOGW(TAG, "disconnected (reason=%u rssi=%d), retry in %" PRIu32 " ms",
                 (unsigned)d->reason, (int)d->rssi, s_backoff_ms);
        esp_event_post(APP_EVENT, APP_EVENT_WIFI_LOST, NULL, 0, 0);
        esp_timer_stop(s_reconnect_timer);
        esp_timer_start_once(s_reconnect_timer, (uint64_t)s_backoff_ms * 1000);
        s_backoff_ms = (s_backoff_ms * 2 > BACKOFF_MAX_MS) ? BACKOFF_MAX_MS : s_backoff_ms * 2;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    if (id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = (const ip_event_got_ip_t *)data;
        snprintf(s_ip_str, sizeof s_ip_str, IPSTR, IP2STR(&e->ip_info.ip));
        s_connected = true;
        s_backoff_ms = BACKOFF_MIN_MS;
        ESP_LOGI(TAG, "got IP %s", s_ip_str);
        esp_event_post(APP_EVENT, APP_EVENT_WIFI_GOT_IP, NULL, 0, 0);
        start_sntp_once();
    }
}

esp_err_t wifi_manager_start(void)
{
    const app_config_t *cfg = app_config_get();

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        return err;
    }
    if (esp_netif_create_default_wifi_sta() == NULL) {
        return ESP_FAIL;
    }

    // Always the macro -- v6 added fields; never brace-init by hand.
    wifi_init_config_t wic = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wic);  // exactly one init, never in the retry path
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return err;
    }

    const esp_timer_create_args_t targs = {
        .callback = reconnect_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wifi_backoff",
    };
    err = esp_timer_create(&targs, &s_reconnect_timer);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t wc = { 0 };
    copy_bytes(wc.sta.ssid, sizeof wc.sta.ssid, cfg->wifi_ssid);
    copy_bytes(wc.sta.password, sizeof wc.sta.password, cfg->wifi_pass);

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &wc);  // WIFI_IF_STA -- ESP_IF_WIFI_STA is removed in v6
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "STA started, ssid=\"%s\"", cfg->wifi_ssid);
    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}

const char *wifi_manager_get_ip_str(void)
{
    return s_ip_str;
}
