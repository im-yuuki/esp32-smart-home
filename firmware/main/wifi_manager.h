// WiFi STA with esp_timer-driven exponential backoff (1 s .. 60 s cap), plus
// the recovery SoftAP for the captive portal (WIFI_MODE_APSTA on demand).
// Posts APP_EVENT_WIFI_GOT_IP / APP_EVENT_WIFI_LOST on the default loop, and
// APP_EVENT_PORTAL_START_REQ on unprovisioned boot or 180 s without an IP.
// Backup-AP semantics: primary first; after every 6 consecutive failures
// alternate primary<->backup; reboot always restarts on primary.
// Starts SNTP once after the first GOT_IP.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi_types.h"  // wifi_ap_record_t

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t   wifi_manager_start(void);
bool        wifi_manager_is_connected(void);
const char *wifi_manager_get_ip_str(void);  // "192.168.1.57", cached from ip_event_got_ip_t
bool        wifi_manager_is_provisioned(void);
const char *wifi_manager_current_ssid(void);  // SSID configured on the STA (primary or backup)
esp_err_t   wifi_manager_ap_start(void);  // idempotent: AP netif + open "SmartHome-Setup-xxxx" + APSTA
esp_err_t   wifi_manager_ap_stop(void);   // back to pure STA; AP netif destroyed
// Blocking (seconds); portal httpd task only. On success *count is the number
// of records written -- 0 with ESP_OK means "no networks found", not an error.
esp_err_t   wifi_manager_scan(wifi_ap_record_t *records, uint16_t *count);

#ifdef __cplusplus
}
#endif
