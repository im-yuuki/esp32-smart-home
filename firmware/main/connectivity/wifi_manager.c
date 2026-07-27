#include "wifi_manager.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

#include "app_config.h"
#include "app_events.h"

static const char *TAG = "wifi";

#define BACKOFF_MIN_MS 1000u
#define BACKOFF_MAX_MS 60000u

#define PORTAL_STA_DOWN_US  (180ULL * 1000 * 1000)  // cumulative no-IP time before the portal opens
#define BACKUP_SWITCH_EVERY 6                       // alternate primary<->backup every N failures

static esp_timer_handle_t s_reconnect_timer;
static esp_timer_handle_t s_downtime_timer;
static uint32_t s_backoff_ms = BACKOFF_MIN_MS;
static volatile bool s_connected;
static bool s_sntp_started;
static char s_ip_str[16] = "0.0.0.0";
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static bool s_provisioned;
static bool s_using_backup;  // inits false => reboot always restarts on primary ("remember nothing")
static uint32_t s_fail_count;
// Set by wifi_manager_scan (httpd task) right before it aborts a mid-flight
// connect, consumed by the disconnect handler (event task): a portal-induced
// disconnect must not count as a connect FAILURE, or six user scans would flip
// the node to the backup AP. Worst case, if the driver never emits the event,
// one genuine disconnect later goes uncounted -- 7 failures before the switch
// instead of 6, which is harmless.
static volatile bool s_scan_disconnect;

// Bounded copy that can never trip -Wformat-truncation (unlike snprintf).
static void copy_bytes(uint8_t *dst, size_t cap, const char *src)
{
    size_t n = strnlen(src, cap - 1);
    memcpy(dst, src, n);
    dst[n] = '\0';
}

// Copy into a fixed-width driver field that reserves NO terminator: the STA
// ssid[32] and password[64] arrays may be fully occupied, and copy_bytes would
// clip a 32-character SSID to 31 or a 64-character PSK to 63 -- either one
// makes association impossible while looking like a typo'd credential. Shorter
// values are zero-padded, which is byte-identical to copy_bytes' output.
static void copy_padded(uint8_t *dst, size_t cap, const char *src)
{
    size_t n = strnlen(src, cap);
    memcpy(dst, src, n);
    memset(dst + n, 0, cap - n);
}

// "Unprovisioned" = wifi_ssid empty or still the Kconfig placeholder. Edge
// case: a real SSID literally equal to the Kconfig default cannot be set via
// menuconfig -- provision it through the portal instead.
static bool provisioned_cfg(const app_config_t *cfg)
{
    return strlen(cfg->wifi_ssid) > 0 && strcmp(cfg->wifi_ssid, CONFIG_SHC_WIFI_SSID) != 0;
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

// esp_timer callback: 180 s without an IP (armed on boot/disconnect, stopped
// ONLY by GOT_IP) -- ask for the recovery portal. The portal ignores the
// request when it is already active.
static void downtime_cb(void *arg)
{
    (void)arg;
    int32_t r = PORTAL_REASON_STA_DOWN;
    esp_event_post(APP_EVENT, APP_EVENT_PORTAL_START_REQ, &r, sizeof r, 0);
}

// Load primary or backup credentials into the STA interface config.
static esp_err_t apply_sta_config(bool use_backup)
{
    const app_config_t *cfg = app_config_get();
    wifi_config_t wc = { 0 };
    copy_padded(wc.sta.ssid, sizeof wc.sta.ssid, use_backup ? cfg->bak_ssid : cfg->wifi_ssid);
    // Password needs the padded copy too: the supplicant reads a 64-character
    // password as a raw hex PMK (strlen == 64 -> hexstr2bin), so clipping it to
    // 63 would silently take the PBKDF2 passphrase branch with wrong material
    // and never associate. The portal accepts exactly 64.
    copy_padded(wc.sta.password, sizeof wc.sta.password, use_backup ? cfg->bak_pass : cfg->wifi_pass);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wc);  // WIFI_IF_STA -- ESP_IF_WIFI_STA is removed in v6
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config(STA) failed: %s", esp_err_to_name(err));
    }
    return err;
}

// ip_mode=static: stop DHCP and program address/mask/gw + DNS on the STA
// netif before esp_wifi_start/first connect. Any parse failure falls back to
// DHCP (a bad static config must not brick the node). IP_EVENT_STA_GOT_IP
// still fires with a static IP, so downstream logic is untouched.
static void apply_static_ip(void)
{
    const app_config_t *cfg = app_config_get();

    esp_err_t err = esp_netif_dhcpc_stop(s_sta_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGE(TAG, "dhcpc_stop failed: %s -- staying on DHCP", esp_err_to_name(err));
        return;
    }

    esp_netif_ip_info_t info = { 0 };
    if (esp_netif_str_to_ip4(cfg->st_ip, &info.ip) != ESP_OK
        || esp_netif_str_to_ip4(cfg->st_nm, &info.netmask) != ESP_OK
        || esp_netif_str_to_ip4(cfg->st_gw, &info.gw) != ESP_OK) {
        ESP_LOGE(TAG, "invalid static IP config (\"%s\"/\"%s\"/\"%s\") -- falling back to DHCP",
                 cfg->st_ip, cfg->st_nm, cfg->st_gw);
        esp_netif_dhcpc_start(s_sta_netif);
        return;
    }
    err = esp_netif_set_ip_info(s_sta_netif, &info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_ip_info failed: %s -- falling back to DHCP", esp_err_to_name(err));
        esp_netif_dhcpc_start(s_sta_netif);
        return;
    }

    esp_netif_dns_info_t dns = { 0 };
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    if (cfg->st_dns[0] == '\0' || esp_netif_str_to_ip4(cfg->st_dns, &dns.ip.u_addr.ip4) != ESP_OK) {
        dns.ip.u_addr.ip4 = info.gw;  // empty (or unparsable) DNS -> use the gateway
    }
    err = esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_dns_info failed: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "static IP %s/%s gw %s dns %s", cfg->st_ip, cfg->st_nm, cfg->st_gw,
             cfg->st_dns[0] != '\0' ? cfg->st_dns : "(gateway)");
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    if (id == WIFI_EVENT_STA_START) {
        if (s_provisioned) {  // unprovisioned STA stays idle so portal scans work
            esp_wifi_connect();
        }
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        if (!s_provisioned) {
            return;  // no credentials to retry with (portal-only boot)
        }
        const wifi_event_sta_disconnected_t *d = (const wifi_event_sta_disconnected_t *)data;
        // Log the reason NUMERICALLY -- several WIFI_REASON_* enums were
        // removed in v6; do not switch on them.
        ESP_LOGW(TAG, "disconnected (reason=%u rssi=%d), retry in %" PRIu32 " ms",
                 (unsigned)d->reason, (int)d->rssi, s_backoff_ms);
        esp_event_post(APP_EVENT, APP_EVENT_WIFI_LOST, NULL, 0, 0);

        // Backup-AP alternation: after every batch of BACKUP_SWITCH_EVERY
        // consecutive failures flip primary<->backup (attempts 1-6 primary,
        // 7-12 backup, 13-18 primary, ...). Backoff timing is untouched.
        // A disconnect we caused ourselves to free the radio for a portal scan
        // is not a connect failure and must not move the counter.
        if (s_scan_disconnect) {
            s_scan_disconnect = false;
            ESP_LOGD(TAG, "disconnect caused by a portal scan -- not counted");
        } else {
            s_fail_count++;
            const app_config_t *cfg = app_config_get();
            if (cfg->bak_ssid[0] != '\0' && (s_fail_count % BACKUP_SWITCH_EVERY) == 0) {
                s_using_backup = !s_using_backup;
                ESP_LOGW(TAG, "%" PRIu32 " consecutive failures -- switching to %s AP \"%s\"",
                         s_fail_count, s_using_backup ? "backup" : "primary",
                         s_using_backup ? cfg->bak_ssid : cfg->wifi_ssid);
                apply_sta_config(s_using_backup);
            }
        }
        // Cumulative-downtime portal trigger: armed on the first disconnect,
        // reset ONLY by GOT_IP (individual retry attempts never reset it).
        if (!esp_timer_is_active(s_downtime_timer)) {
            esp_timer_start_once(s_downtime_timer, PORTAL_STA_DOWN_US);
        }
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
        s_fail_count = 0;
        esp_timer_stop(s_downtime_timer);  // the only place the downtime clock resets
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
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        return ESP_FAIL;
    }

    s_provisioned = provisioned_cfg(cfg);
    if (s_provisioned && cfg->ip_mode == 1) {
        apply_static_ip();
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
    const esp_timer_create_args_t dtargs = {
        .callback = downtime_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "portal_downtime",
    };
    err = esp_timer_create(&dtargs, &s_downtime_timer);
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

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }
    err = apply_sta_config(false);  // always start on the primary AP
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }

    if (s_provisioned) {
        // Arm the no-IP portal trigger from boot; GOT_IP stops it.
        esp_timer_start_once(s_downtime_timer, PORTAL_STA_DOWN_US);
        ESP_LOGI(TAG, "STA started, ssid=\"%s\"", cfg->wifi_ssid);
    } else {
        // Posting after esp_wifi_start guarantees WiFi is up when the portal
        // handler (registered earlier in app_main) processes the request.
        ESP_LOGW(TAG, "unprovisioned (SSID empty or Kconfig placeholder) -- requesting portal");
        int32_t r = PORTAL_REASON_UNPROVISIONED;
        esp_event_post(APP_EVENT, APP_EVENT_PORTAL_START_REQ, &r, sizeof r, 0);
    }
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

bool wifi_manager_is_provisioned(void)
{
    return s_provisioned;
}

const char *wifi_manager_current_ssid(void)
{
    const app_config_t *cfg = app_config_get();
    return s_using_backup ? cfg->bak_ssid : cfg->wifi_ssid;
}

esp_err_t wifi_manager_ap_start(void)
{
    if (s_ap_netif != NULL) {
        return ESP_OK;  // idempotent
    }
    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (s_ap_netif == NULL) {
        return ESP_FAIL;
    }
    // Mode first: esp_wifi_set_config(WIFI_IF_AP) needs the AP active.
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
        return err;
    }

    // GD3 naming: "SmartHome-Setup-" + last 4 hex of the MAC (same MAC family
    // as node_id).
    uint8_t mac[6] = { 0 };
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char ssid[33];
    snprintf(ssid, sizeof ssid, "SmartHome-Setup-%02x%02x", mac[4], mac[5]);

    wifi_config_t ap = {
        .ap = {
            .ssid_len = (uint8_t)strlen(ssid),
            .channel = 1,  // in APSTA the AP hops to the STA channel on STA connect -- expected
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,  // open by design -- see the security notes in portal.c
        },
    };
    copy_bytes(ap.ap.ssid, sizeof ap.ap.ssid, ssid);
    err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config(AP) failed: %s", esp_err_to_name(err));
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
        return err;
    }
    // AP netif keeps the IDF defaults: IP 192.168.4.1, DHCP server auto-start.
    ESP_LOGI(TAG, "SoftAP up: \"%s\" (open), AP IP 192.168.4.1", ssid);
    return ESP_OK;
}

// ORDERING IS LOAD-BEARING -- do not reorder these three calls.
//
// The DHCP server's udp_pcb is only released by dhcps_stop(), which the netif
// reaches via esp_netif_action_stop(). Relying on WIFI_EVENT_AP_STOP to get
// there does not work here: this function runs on the default event-loop task
// (portal_do_stop), so AP_STOP cannot be dispatched until we return, and by
// then esp_netif_destroy_default_wifi() has already cleared the driver's
// AP netif slot -- wifi_default_action_ap_stop() then no-ops. dhcps_delete()
// on a still-STARTED server merely marks it DELETE_PENDING and the timer later
// frees the struct without ever touching dhcps_pcb, so each portal cycle would
// leak one udp_pcb plus its lease list. With CONFIG_LWIP_MAX_UDP_PCBS=16 and
// ~5 in steady use, ~11 cycles exhaust the pool: the SoftAP still comes up but
// hands out no addresses and the DNS-hijack socket cannot bind -- the recovery
// portal is unreachable until a reflash.
//
// So: stop the netif FIRST (while it is still up and still bound to the
// driver, so dhcps_stop() runs and udp_remove()s the pcb), then leave APSTA,
// then destroy.
esp_err_t wifi_manager_ap_stop(void)
{
    if (s_ap_netif == NULL) {
        return ESP_OK;
    }
    esp_netif_action_stop(s_ap_netif, NULL, 0, NULL);  // frees the dhcps pcb -- must precede the mode change
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_mode(STA) failed: %s", esp_err_to_name(err));
    }
    esp_netif_destroy_default_wifi(s_ap_netif);
    s_ap_netif = NULL;
    ESP_LOGI(TAG, "SoftAP stopped");
    return err;
}

esp_err_t wifi_manager_scan(wifi_ap_record_t *records, uint16_t *count)
{
    if (records == NULL || count == NULL || *count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    // Prevent a backoff-timer reconnect from aborting the scan mid-flight.
    esp_timer_stop(s_reconnect_timer);

    esp_err_t err = esp_wifi_scan_start(NULL, true);  // blocking
    if (err == ESP_ERR_WIFI_STATE && !s_connected) {
        // STA mid-connect: abort the attempt and retry once. Never when we are
        // already connected -- tearing down a live link (and the MQTT session
        // on it) just to serve /api/scan is not a trade worth making; the UI
        // shows the error and the user can retry.
        // Flag first: the event can be dispatched before the call returns.
        s_scan_disconnect = true;
        if (esp_wifi_disconnect() != ESP_OK) {
            s_scan_disconnect = false;  // no STA_DISCONNECTED will follow
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        err = esp_wifi_scan_start(NULL, true);
    }
    if (err == ESP_OK) {
        uint16_t n = 0;
        esp_wifi_scan_get_ap_num(&n);
        if (n < *count) {
            *count = n;
        }
        if (*count == 0) {
            // Nothing found. esp_wifi_scan_get_ap_records() rejects *number==0
            // with ESP_ERR_INVALID_ARG, which the portal would surface as a 500
            // "scan_failed"; an empty list is a normal result the UI renders as
            // "No networks found".
            esp_wifi_clear_ap_list();
        } else {
            err = esp_wifi_scan_get_ap_records(count, records);  // frees driver memory
        }
    } else {
        esp_wifi_clear_ap_list();  // free driver memory on the failure path too
        *count = 0;
    }

    if (s_provisioned && !s_connected) {
        esp_timer_stop(s_reconnect_timer);  // may have been re-armed by a disconnect meanwhile
        esp_timer_start_once(s_reconnect_timer, 1000ULL * 1000);  // resume retries
    }
    return err;
}
