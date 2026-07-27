// Recovery/provisioning captive portal: SoftAP + DNS hijack + embedded UI.
//
// Lifecycle: portal_init() only registers APP_EVENT handlers and creates
// timers. httpd + DNS + AP netif exist ONLY between PORTAL_START_REQ and stop
// (minimizes attack surface). All start/stop work executes in the default
// event-loop task, so it is serialized by construction; timers only post
// events. Stop policy: only a STA_DOWN-triggered portal auto-stops (60 s
// grace after STA recovers); UNPROVISIONED/MANUAL portals stay up until
// Save & Reboot.
//
// Security model (deliberate decisions, not oversights):
// - OPEN AP: recovery must work when credentials are lost/wrong; a WPA2 AP
//   with a password printed nowhere would brick recovery. Compensating
//   controls: the portal exists only while active and is torn down fully
//   afterwards; every /api/* call except login needs the session; 1 s
//   lockout after a failed login; 192.168.4.x is proximity-local.
// - Default password admin/CONFIG_SHC_ADMIN_PASS seeded from Kconfig; the UI
//   shows a warning banner until it is changed (default_pass flag).
// - Session: 128-bit esp_fill_random token, RAM-only (dies on reboot), logout
//   endpoint, single session (new login evicts). The cookie carries NO
//   Max-Age -- it is a session cookie, and the server-side 10 min IDLE timer
//   is the only expiry. A browser-side lifetime would log the user out (and
//   wipe the form they were filling in) while they were actively typing.
// - AP-only API: httpd binds INADDR_ANY unconditionally (httpd_config_t has no
//   netif option in IDF v6), so while the portal runs in APSTA with a working
//   STA the listener is also reachable from the home LAN through the STA
//   address. Every /api/* request -- login included -- is therefore required to
//   be addressed TO the SoftAP IP and to come FROM the SoftAP subnet (see
//   req_is_from_ap for why both halves are needed). Only "/" and the
//   captive-probe 302 stay open.
// - Honest limitation: credentials transit plaintext HTTP over an open AP
//   during provisioning -- inherent to captive-portal provisioning on this
//   class of device; GD3 may revisit.
#include "portal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>  // sockaddr_in / sockaddr_in6
#include <sys/socket.h>  // getsockname/getpeername (lwIP POSIX shims)

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "dns_server.h"  // after esp_netif.h: the vendored header needs esp_ip4_addr_t
#include "esp_random.h"
#include "esp_system.h"  // esp_restart
#include "esp_timer.h"

#include "sdkconfig.h"  // CONFIG_HTTPD_MAX_REQ_HDR_LEN (see COOKIE_CAP)

#include "app_config.h"
#include "app_events.h"
#include "json_guard.h"
#include "mqtt_mgr.h"
#include "relay_driver.h"
#include "sensor_task.h"
#include "wifi_manager.h"

static const char *TAG = "portal";

#define PORTAL_GRACE_US        (60ULL * 1000 * 1000)   // portal lifetime after STA recovery
#define SESSION_IDLE_US        (600LL * 1000 * 1000)   // 10 min idle -> session expires
#define LOGIN_LOCKOUT_US       (1000LL * 1000)         // 1 s between login attempts after a failure
#define PORTAL_REBOOT_DELAY_US (1500LL * 1000)         // let the response + offline publish flush
#define PORTAL_HTTPD_STACK     8192                    // bytes (IDF, not words)
#define SCAN_MAX_APS           20
#define BODY_CAP               1024                    // request-body cap for every POST
// Cookie header value buffer; see check_auth. MUST be >=
// CONFIG_HTTPD_MAX_REQ_HDR_LEN: httpd accepts header blocks up to that size, so
// anything smaller leaves a window in which a real phone's cookie jar does not
// fit and the session can never be matched.
#define COOKIE_CAP             1024
_Static_assert(COOKIE_CAP >= CONFIG_HTTPD_MAX_REQ_HDR_LEN,
               "COOKIE_CAP must cover every Cookie value httpd will accept");

// SoftAP netif address (IDF default for esp_netif_create_default_wifi_ap).
// Single source of truth: captive redirect, logs and the AP-only gate.
#define AP_IP_STR  "192.168.4.1"
#define AP_IP_ADDR ESP_IP4TOADDR(192, 168, 4, 1)  // same value, network byte order

// EMBED_TXTFILES appends a NUL; exclude it from the response length.
extern const char portal_start[] asm("_binary_portal_html_start");
extern const char portal_end[] asm("_binary_portal_html_end");

static httpd_handle_t s_httpd;
static dns_server_handle_t s_dns;
static bool s_active;
static portal_reason_t s_reason;
static esp_timer_handle_t s_grace_timer, s_reboot_timer;
static int64_t s_last_login_fail_us;

static struct {
    char    token[33];         // 32 lowercase hex chars + NUL
    int64_t last_activity_us;
    bool    valid;
} s_session;

// ---------------------------------------------------------------- helpers --

static esp_err_t send_err_json(httpd_req_t *req, const char *status, const char *json)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

// Serialize, send and free a cJSON tree (consumes root even on failure).
static esp_err_t send_json(httpd_req_t *req, cJSON *root)
{
    char *txt = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (txt == NULL) {
        return send_err_json(req, "500 Internal Server Error", "{\"error\":\"no_mem\"}");
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, txt);
    cJSON_free(txt);
    return err;
}

// IPv4 address (network byte order) of a socket address, 0 when it is neither
// AF_INET nor an IPv4-mapped AF_INET6 one. Dual-stack lwIP (CONFIG_LWIP_IPV6=y)
// makes httpd listen on AF_INET6 and reports IPv4 peers as ::ffff:a.b.c.d, so
// both shapes must be handled.
static uint32_t sockaddr_ipv4(const struct sockaddr_storage *sa)
{
    if (sa->ss_family == AF_INET) {
        return ((const struct sockaddr_in *)sa)->sin_addr.s_addr;
    }
    if (sa->ss_family == AF_INET6) {
        static const uint8_t v4_mapped[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };
        const uint8_t *b = ((const struct sockaddr_in6 *)sa)->sin6_addr.s6_addr;
        if (memcmp(b, v4_mapped, sizeof v4_mapped) == 0) {
            uint32_t v4;
            memcpy(&v4, b + 12, sizeof v4);
            return v4;
        }
    }
    return 0;
}

// Interface gate for /api/*. httpd always binds INADDR_ANY (no netif option in
// IDF v6), so in APSTA the same listener is reachable through the STA address;
// this gate is what keeps the API -- login included -- off the home LAN.
//
// The invariant is NOT "local socket address == 192.168.4.1 means the request
// arrived on the SoftAP netif". lwIP's ip4_input accepts a datagram received on
// ANY netif as long as SOME netif owns the destination address: when the
// arrival netif does not accept it, ip4_input walks NETIF_FOREACH and takes the
// first netif that does (ip4.c:613-640; this build has LWIP_SINGLE_NETIF=0,
// LWIP_HAVE_LOOPIF=1 and no LWIP_HOOK_IP4_INPUT). A LAN host that ARP- or
// route-pins 192.168.4.1 to the node's STA MAC therefore produces a pcb whose
// local address is exactly the AP IP. The local check alone is only a statement
// about which address was targeted, not about which link carried the frame.
//
// What makes the gate sound is the PEER check: 192.168.4.0/24 is handed out
// solely by the SoftAP's own DHCP server, so a source address inside the AP
// subnet is reachable only by a station associated to the AP. Both halves are
// required -- the local check rejects LAN traffic aimed at the STA address, the
// peer check rejects LAN traffic aimed at the AP address.
//
// (IDF's LWIP_HOOK_IP4_ROUTE_SRC happens to force the SYN-ACK back out the AP
// netif today, which is why the LAN handshake fails in practice. That is an
// unrelated mechanism this code neither references nor controls, so it is not
// what the gate rests on.)
//
// Fail-closed: any getsockname/getpeername/netif lookup failure is a reject.
// Logs once per rejected request; the response is the caller's job.
static bool req_is_from_ap(httpd_req_t *req)
{
    int fd = httpd_req_to_sockfd(req);
    struct sockaddr_storage sa;
    socklen_t len;
    uint32_t local = 0, peer = 0;  // network byte order; 0 = unknown

    if (fd >= 0) {
        len = sizeof sa;
        if (getsockname(fd, (struct sockaddr *)&sa, &len) == 0) {
            local = sockaddr_ipv4(&sa);
        }
        len = sizeof sa;
        if (getpeername(fd, (struct sockaddr *)&sa, &len) == 0) {
            peer = sockaddr_ipv4(&sa);
        }
    }

    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t info = { 0 };
    bool have_ap = ap != NULL && esp_netif_get_ip_info(ap, &info) == ESP_OK && info.netmask.addr != 0;

    // peer != 0 is load-bearing, not a nicety: 0 is also what sockaddr_ipv4()
    // returns for "unknown peer" (getpeername failed, or a non-IPv4 family).
    // If info.ip.addr were ever 0 while info.netmask.addr stayed non-zero, the
    // subnet equality below would hold for that unknown peer and open the gate.
    if (have_ap && local == AP_IP_ADDR && peer != 0
        && (peer & info.netmask.addr) == (info.ip.addr & info.netmask.addr)) {
        return true;
    }
    esp_ip4_addr_t p = { .addr = peer };  // stays 0.0.0.0 if the peer is unknown
    ESP_LOGW(TAG, "rejected %s from " IPSTR " -- not on the SoftAP interface", req->uri, IP2STR(&p));
    return false;
}

// Locate our session token inside a Cookie header value, anchored to a cookie
// NAME boundary (start of the header, or just after a ';' separator) so a
// cookie called e.g. "xshc_sess" cannot be taken for ours. Returns a pointer to
// the value, or NULL. Not exploitable -- the value still has to match the
// 128-bit token -- just correct.
static const char *find_session_cookie(const char *cookies)
{
    static const char name[] = "shc_sess=";
    const size_t nlen = sizeof name - 1;
    for (const char *p = strstr(cookies, name); p != NULL; p = strstr(p + nlen, name)) {
        // Walk back over the whitespace RFC 6265 allows around the "; "
        // separator. Running off the front of the value means the match starts
        // the header (possibly after leading spaces), which is a name boundary
        // too -- the earlier version compared the character it stopped ON and so
        // missed "  shc_sess=..." entirely.
        const char *q = p;
        while (q > cookies && (q[-1] == ' ' || q[-1] == '\t')) {
            q--;
        }
        if (q == cookies || q[-1] == ';') {
            return p + nlen;
        }
    }
    return NULL;
}

// Cookie-session gate for every /api/* endpoint except login. On failure the
// 401/403 response has already been sent; the handler must just return ESP_OK.
static bool check_auth(httpd_req_t *req)
{
    if (!req_is_from_ap(req)) {
        send_err_json(req, "403 Forbidden", "{\"error\":\"ap_only\"}");
        return false;
    }
    // httpd accepts headers up to CONFIG_HTTPD_MAX_REQ_HDR_LEN and 192.168.4.1
    // is the most-reused captive-portal address in existence, so phones show up
    // with a pile of cookies scoped to it. A buffer that a real Cookie header
    // overflows -- combined with "only ESP_OK counts" -- turns into a
    // login-then-kick loop: the login succeeds, then every /api/* 401s forever,
    // silently and unrecoverably. COOKIE_CAP therefore covers the whole header
    // block httpd will accept (static assert at its definition), so the value
    // always fits; truncation is still accepted below because the underlying
    // strlcpy always NUL-terminates and a prefix match is strictly better than
    // a hard failure.
    //
    // static, not on the stack: every httpd handler runs on the one server task
    // (esp_http_server/src/httpd_main.c creates a single httpd_thread per
    // server and dispatches sessions from it in a loop; there is no worker pool
    // and CONFIG_HTTPD_WS_SUPPORT is off), so check_auth can never be entered
    // twice concurrently. That keeps 1 KB off an 8 KB task stack that already
    // carries a 1 KB body buffer in the calling handler's own frame.
    static char cookie[COOKIE_CAP];
    bool ok = false;
    esp_err_t herr = httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof cookie);
    if (herr == ESP_ERR_HTTPD_RESULT_TRUNC) {
        ESP_LOGD(TAG, "Cookie header longer than %u bytes -- matching against the prefix",
                 (unsigned)(sizeof cookie - 1));
    }
    if (s_session.valid && (herr == ESP_OK || herr == ESP_ERR_HTTPD_RESULT_TRUNC)) {
        const char *tok = find_session_cookie(cookie);
        if (tok != NULL && strncmp(tok, s_session.token, 32) == 0) {
            int64_t now = esp_timer_get_time();
            if (now - s_session.last_activity_us <= SESSION_IDLE_US) {
                s_session.last_activity_us = now;
                ok = true;
            } else {
                s_session.valid = false;  // idle expiry
            }
        }
    }
    if (!ok) {
        send_err_json(req, "401 Unauthorized", "{\"error\":\"unauthorized\"}");
    }
    return ok;
}

// Receive and parse a JSON body. On failure the error response has already
// been sent (or the socket is dead) -- the handler must just return ESP_OK.
static esp_err_t read_json_body(httpd_req_t *req, char *buf, size_t cap, cJSON **out)
{
    *out = NULL;
    if (req->content_len == 0) {
        send_err_json(req, "400 Bad Request", "{\"error\":\"empty_body\"}");
        return ESP_FAIL;
    }
    if (req->content_len >= cap) {
        send_err_json(req, "413 Payload Too Large", "{\"error\":\"too_large\"}");
        return ESP_FAIL;
    }
    size_t got = 0;
    while (got < req->content_len) {
        int r = httpd_req_recv(req, buf + got, req->content_len - got);
        if (r <= 0) {
            return ESP_FAIL;  // socket error/timeout; connection is gone
        }
        got += (size_t)r;
    }
    buf[got] = '\0';
    if (!json_depth_ok_n(buf, got)) {
        ESP_LOGW(TAG, "rejected over-nested JSON body on %s", req->uri);
        send_err_json(req, "400 Bad Request", "{\"error\":\"bad_json\"}");
        return ESP_FAIL;
    }
    *out = cJSON_Parse(buf);
    if (*out == NULL) {
        send_err_json(req, "400 Bad Request", "{\"error\":\"bad_json\"}");
        return ESP_FAIL;
    }
    return ESP_OK;
}

// Consume an unused request body (logout/reboot POSTs) so keep-alive parsing
// of the next request stays in sync.
static void drain_body(httpd_req_t *req)
{
    char sink[64];
    size_t left = req->content_len;
    while (left > 0) {
        int r = httpd_req_recv(req, sink, left < sizeof sink ? left : sizeof sink);
        if (r <= 0) {
            break;
        }
        left -= (size_t)r;
    }
}

static const char *json_str(const cJSON *obj, const char *key)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return (cJSON_IsString(it) && it->valuestring != NULL) ? it->valuestring : NULL;
}

// A netmask must be a non-empty, contiguous run of 1 bits followed by 0 bits.
// ~m + 1 is a power of two exactly when m has that shape.
static bool netmask_is_contiguous(uint32_t mask_net_order)
{
    uint32_t m = ntohl(mask_net_order);
    if (m == 0) {
        return false;
    }
    uint32_t inv = ~m;
    return (inv & (inv + 1)) == 0;
}

// Arithmetic sanity for a static host config (all arguments network byte
// order). "It parses" is not enough -- see the note in h_cfg_post for why a
// merely well-formed config is a lockout. Rejects, in order: a zero or
// non-contiguous netmask; anything longer than /30 (a /31 or /32 leaves no
// room for a host plus a gateway); a gateway outside the host's own subnet;
// gateway == ip; and the two addresses that are not host addresses at all,
// the subnet's network and broadcast address.
static bool static_ip_is_unusable(uint32_t ip_n, uint32_t nm_n, uint32_t gw_n)
{
    if (!netmask_is_contiguous(nm_n)) {
        return true;
    }
    uint32_t host_bits = ~ntohl(nm_n);  // 0 for /32, 1 for /31, 3 for /30, ...
    if (host_bits < 3) {
        return true;
    }
    if ((ip_n & nm_n) != (gw_n & nm_n) || ip_n == gw_n) {
        return true;
    }
    uint32_t host = ntohl(ip_n) & host_bits;
    return host == 0 || host == host_bits;  // network / broadcast address
}

// Catch the dominant lockout typo: a static config for a DIFFERENT network than
// the one the node is joined to right now (192.168.0.50/24 gw 192.168.0.1 typed
// on a 192.168.1.0/24 LAN). It is internally consistent, so every check above
// passes, and it still produces IP_EVENT_STA_GOT_IP -- the only event that
// resets the 180 s downtime clock -- leaving the node on WiFi, unreachable,
// MQTT dead and the auto-portal disarmed.
//
// Only meaningful while the STA is associated, and only then is the live
// ip_info trustworthy; with the STA down the user may legitimately be
// reconfiguring for a different network, so the check is skipped rather than
// guessed at. Same for a netif we cannot read: never block on missing data.
static bool sta_subnet_mismatch(uint32_t ip_n, uint32_t nm_n)
{
    if (!wifi_manager_is_connected()) {
        return false;
    }
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t live = { 0 };
    if (sta == NULL || esp_netif_get_ip_info(sta, &live) != ESP_OK
        || live.ip.addr == 0 || live.netmask.addr == 0) {
        return false;
    }
    return (ip_n & nm_n) != (live.ip.addr & live.netmask.addr);
}

static const char *auth_str(wifi_auth_mode_t m)
{
    switch (m) {
    case WIFI_AUTH_OPEN:
        return "OPEN";
    case WIFI_AUTH_WEP:
        return "WEP";
    case WIFI_AUTH_WPA_PSK:
        return "WPA";
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "WPA2/WPA3";
    default:
        return "SECURED";
    }
}

// --------------------------------------------------------------- handlers --

static esp_err_t h_root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, portal_start, portal_end - portal_start - 1);
}

static esp_err_t h_login_post(httpd_req_t *req)
{
    // Before the lockout check on purpose: a LAN probe must not be able to
    // park a legitimate AP client in the 1 s rate limit.
    if (!req_is_from_ap(req)) {
        return send_err_json(req, "403 Forbidden", "{\"error\":\"ap_only\"}");
    }
    if (esp_timer_get_time() - s_last_login_fail_us < LOGIN_LOCKOUT_US) {
        return send_err_json(req, "429 Too Many Requests", "{\"error\":\"rate_limited\"}");
    }
    char buf[BODY_CAP];
    cJSON *root;
    if (read_json_body(req, buf, sizeof buf, &root) != ESP_OK) {
        return ESP_OK;
    }
    const char *user = json_str(root, "username");
    const char *pass = json_str(root, "password");
    const app_config_t *cfg = app_config_get();
    bool ok = user != NULL && strcmp(user, "admin") == 0
              && pass != NULL && strcmp(pass, cfg->admin_pass) == 0;
    cJSON_Delete(root);
    if (!ok) {
        s_last_login_fail_us = esp_timer_get_time();
        return send_err_json(req, "401 Unauthorized", "{\"error\":\"bad_credentials\"}");
    }

    // Single session, last login wins. Token dies with the portal/reboot.
    uint8_t raw[16];
    esp_fill_random(raw, sizeof raw);
    for (size_t i = 0; i < sizeof raw; i++) {
        snprintf(&s_session.token[i * 2], 3, "%02x", (unsigned)raw[i]);
    }
    s_session.valid = true;
    s_session.last_activity_us = esp_timer_get_time();

    // HttpOnly keeps the token out of scripts; SameSite=Strict is the only
    // CSRF control we have (there is no per-form token) and blocks the
    // cross-site POSTs a captive mini-browser would otherwise carry.
    //
    // NO Max-Age on purpose -- a session cookie. The documented expiry is
    // *idle* expiry and lives server-side (SESSION_IDLE_US, refreshed by every
    // authenticated request). A browser-side lifetime is wall-clock, not idle:
    // it dropped the cookie 10 min after login no matter how active the user
    // was, the next 2 s status poll 401'd, the UI fell back to the login screen
    // and re-login re-ran prefill(), overwriting every field typed since. The
    // cookie is RAM-only anyway (the token dies with the portal and the reboot).
    char cookie[96];  // stays in scope until the response is sent below
    snprintf(cookie, sizeof cookie, "shc_sess=%s; Path=/; HttpOnly; SameSite=Strict",
             s_session.token);
    httpd_resp_set_hdr(req, "Set-Cookie", cookie);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, app_config_admin_pass_is_default()
                                       ? "{\"ok\":true,\"default_pass\":true}"
                                       : "{\"ok\":true,\"default_pass\":false}");
}

static esp_err_t h_logout_post(httpd_req_t *req)
{
    if (!check_auth(req)) {
        return ESP_OK;
    }
    drain_body(req);
    memset(&s_session, 0, sizeof s_session);
    // Max-Age=0 stays HERE (and only here): it is the delete directive, and it
    // still matches the session cookie set at login on name/path/attributes.
    httpd_resp_set_hdr(req, "Set-Cookie",
                       "shc_sess=deleted; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t h_status_get(httpd_req_t *req)
{
    if (!check_auth(req)) {
        return ESP_OK;
    }
    const app_config_t *cfg = app_config_get();
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return send_err_json(req, "500 Internal Server Error", "{\"error\":\"no_mem\"}");
    }
    cJSON_AddStringToObject(root, "node_id", cfg->node_id);
    cJSON_AddStringToObject(root, "fw", esp_app_get_description()->version);
    cJSON *sta = cJSON_AddObjectToObject(root, "sta");
    cJSON_AddBoolToObject(sta, "connected", wifi_manager_is_connected());
    cJSON_AddStringToObject(sta, "ip", wifi_manager_get_ip_str());
    cJSON_AddStringToObject(sta, "ssid", wifi_manager_current_ssid());
    cJSON_AddBoolToObject(root, "mqtt", mqtt_mgr_is_connected());
    cJSON_AddBoolToObject(root, "sensor", sensor_present());
    cJSON *relays = cJSON_AddArrayToObject(root, "relays");
    for (uint8_t ch = 1; ch <= cfg->relay_count; ch++) {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddNumberToObject(r, "ch", ch);
        cJSON_AddStringToObject(r, "name", cfg->relay_name[ch - 1]);
        cJSON_AddBoolToObject(r, "state", relay_driver_get_state(ch));
        cJSON_AddItemToArray(relays, r);
    }
    cJSON_AddBoolToObject(root, "default_pass", app_config_admin_pass_is_default());
    cJSON_AddBoolToObject(root, "provisioned", wifi_manager_is_provisioned());
    return send_json(req, root);
}

static esp_err_t h_scan_get(httpd_req_t *req)
{
    if (!check_auth(req)) {
        return ESP_OK;
    }
    uint16_t count = SCAN_MAX_APS;
    wifi_ap_record_t *recs = calloc(SCAN_MAX_APS, sizeof(wifi_ap_record_t));
    if (recs == NULL) {
        return send_err_json(req, "500 Internal Server Error", "{\"error\":\"no_mem\"}");
    }
    esp_err_t err = wifi_manager_scan(recs, &count);  // blocking, seconds -- fine on the httpd task
    if (err != ESP_OK) {
        free(recs);
        ESP_LOGW(TAG, "scan failed: %s", esp_err_to_name(err));
        return send_err_json(req, "500 Internal Server Error", "{\"error\":\"scan_failed\"}");
    }
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        free(recs);
        return send_err_json(req, "500 Internal Server Error", "{\"error\":\"no_mem\"}");
    }
    cJSON *aps = cJSON_AddArrayToObject(root, "aps");
    for (uint16_t i = 0; i < count; i++) {
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", (const char *)recs[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", recs[i].rssi);
        cJSON_AddStringToObject(ap, "auth", auth_str(recs[i].authmode));
        cJSON_AddItemToArray(aps, ap);
    }
    free(recs);
    return send_json(req, root);
}

static esp_err_t h_cfg_get(httpd_req_t *req)
{
    if (!check_auth(req)) {
        return ESP_OK;
    }
    const app_config_t *cfg = app_config_get();
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return send_err_json(req, "500 Internal Server Error", "{\"error\":\"no_mem\"}");
    }
    // Non-secret prefill only -- passwords are NEVER echoed back.
    cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
    cJSON_AddStringToObject(wifi, "ssid", wifi_manager_is_provisioned() ? cfg->wifi_ssid : "");
    cJSON_AddStringToObject(wifi, "bak_ssid", cfg->bak_ssid);
    cJSON *ip = cJSON_AddObjectToObject(root, "ip");
    cJSON_AddStringToObject(ip, "mode", cfg->ip_mode == 1 ? "static" : "dhcp");
    cJSON_AddStringToObject(ip, "ip", cfg->st_ip);
    cJSON_AddStringToObject(ip, "netmask", cfg->st_nm);
    cJSON_AddStringToObject(ip, "gateway", cfg->st_gw);
    cJSON_AddStringToObject(ip, "dns", cfg->st_dns);
    cJSON *mq = cJSON_AddObjectToObject(root, "mqtt");
    cJSON_AddStringToObject(mq, "uri", cfg->mqtt_uri);
    cJSON_AddStringToObject(mq, "user", cfg->mqtt_user);
    return send_json(req, root);
}

// POST /api/config -- every object/field optional; empty-string passwords
// mean "keep current"; empty bak_ssid clears the backup AP. Validates
// everything BEFORE writing anything, then saves per section. No reboot here
// (Save & Reboot in the UI follows with /api/reboot).
static esp_err_t h_cfg_post(httpd_req_t *req)
{
    if (!check_auth(req)) {
        return ESP_OK;
    }
    char buf[BODY_CAP];
    cJSON *root;
    if (read_json_body(req, buf, sizeof buf, &root) != ESP_OK) {
        return ESP_OK;
    }

    bool too_long = false;
    bool bad_ip = false;
    bool bad_subnet = false;
    bool wrong_subnet = false;

    const cJSON *wifi = cJSON_GetObjectItemCaseSensitive(root, "wifi");
    const char *w_ssid = NULL, *w_pass = NULL, *w_bssid = NULL, *w_bpass = NULL;
    if (cJSON_IsObject(wifi)) {
        w_ssid = json_str(wifi, "ssid");
        w_pass = json_str(wifi, "pass");
        w_bssid = json_str(wifi, "bak_ssid");  // "" clears the backup AP
        w_bpass = json_str(wifi, "bak_pass");
        if (w_ssid != NULL && w_ssid[0] == '\0') {
            w_ssid = NULL;  // empty = keep (primary SSID cannot be cleared)
        }
        if (w_pass != NULL && w_pass[0] == '\0') {
            w_pass = NULL;
        }
        if (w_bpass != NULL && w_bpass[0] == '\0') {
            w_bpass = NULL;
        }
        too_long = too_long || (w_ssid != NULL && strlen(w_ssid) > 32)
                   || (w_pass != NULL && strlen(w_pass) > 64)
                   || (w_bssid != NULL && strlen(w_bssid) > 32)
                   || (w_bpass != NULL && strlen(w_bpass) > 64);
    }

    const cJSON *ip = cJSON_GetObjectItemCaseSensitive(root, "ip");
    bool have_ip = false;
    uint8_t ip_mode = 0;
    const char *st_ip = NULL, *st_nm = NULL, *st_gw = NULL, *st_dns = NULL;
    if (cJSON_IsObject(ip)) {
        const char *mode = json_str(ip, "mode");
        if (mode != NULL && strcmp(mode, "static") == 0) {
            ip_mode = 1;
            have_ip = true;
            st_ip = json_str(ip, "ip");
            st_nm = json_str(ip, "netmask");
            st_gw = json_str(ip, "gateway");
            st_dns = json_str(ip, "dns");
            esp_ip4_addr_t a_ip = { 0 }, a_nm = { 0 }, a_gw = { 0 }, a_dns = { 0 };
            bad_ip = st_ip == NULL || strlen(st_ip) > 15 || esp_netif_str_to_ip4(st_ip, &a_ip) != ESP_OK
                     || st_nm == NULL || strlen(st_nm) > 15 || esp_netif_str_to_ip4(st_nm, &a_nm) != ESP_OK
                     || st_gw == NULL || strlen(st_gw) > 15 || esp_netif_str_to_ip4(st_gw, &a_gw) != ESP_OK;
            if (st_dns != NULL && st_dns[0] != '\0') {
                bad_ip = bad_ip || strlen(st_dns) > 15 || esp_netif_str_to_ip4(st_dns, &a_dns) != ESP_OK;
            } else {
                st_dns = "";  // empty = use gateway at apply time
            }
            // Parsing is not enough. A well-formed but wrong static config (say
            // 192.168.0.50/24 gw 192.168.0.1 on a 192.168.1.0/24 LAN) still
            // produces IP_EVENT_STA_GOT_IP -- and GOT_IP is the ONLY thing that
            // resets the 180 s downtime clock. The node would sit on WiFi,
            // unreachable, MQTT dead, with the portal auto-trigger permanently
            // disarmed; recovery would need a physical 5 s BOOT hold. So the
            // arithmetic must hold (static_ip_is_unusable) AND, when the STA is
            // currently associated, the address must be on the network the node
            // is actually joined to (sta_subnet_mismatch) -- the two are
            // reported separately so the UI can say which one it was.
            //
            // The live-network check is skipped when this same request also sets
            // a new primary SSID: the user is moving the node to another network
            // and the static address belongs to THAT network, not the one we are
            // joined to right now. Rejecting it would be wrong and would strand
            // the user -- after the move the node is connected, so the portal
            // does not auto-start and a physical BOOT hold would be the only way
            // back in. Arithmetic validation still applies in that case.
            if (!bad_ip) {
                bad_subnet = static_ip_is_unusable(a_ip.addr, a_nm.addr, a_gw.addr);
                if (!bad_subnet && w_ssid == NULL) {
                    wrong_subnet = sta_subnet_mismatch(a_ip.addr, a_nm.addr);
                }
            }
        } else if (mode != NULL && strcmp(mode, "dhcp") == 0) {
            ip_mode = 0;
            have_ip = true;  // addresses left untouched (NULLs below)
        }
    }

    const cJSON *mq = cJSON_GetObjectItemCaseSensitive(root, "mqtt");
    const char *m_uri = NULL, *m_user = NULL, *m_pass = NULL;
    if (cJSON_IsObject(mq)) {
        m_uri = json_str(mq, "uri");
        m_user = json_str(mq, "user");
        m_pass = json_str(mq, "pass");
        if (m_uri != NULL && m_uri[0] == '\0') {
            m_uri = NULL;  // empty = keep (UI prefills current values)
        }
        if (m_user != NULL && m_user[0] == '\0') {
            m_user = NULL;
        }
        if (m_pass != NULL && m_pass[0] == '\0') {
            m_pass = NULL;
        }
        too_long = too_long || (m_uri != NULL && strlen(m_uri) > 127)
                   || (m_user != NULL && strlen(m_user) > 32)
                   || (m_pass != NULL && strlen(m_pass) > 64);
    }

    if (too_long || bad_ip || bad_subnet || wrong_subnet) {
        cJSON_Delete(root);
        const char *body = too_long     ? "{\"error\":\"too_long\"}"
                           : bad_ip     ? "{\"error\":\"bad_ip\"}"
                           : bad_subnet ? "{\"error\":\"bad_subnet\"}"
                                        : "{\"error\":\"wrong_subnet\"}";
        return send_err_json(req, "400 Bad Request", body);
    }

    esp_err_t err = ESP_OK;
    if (w_ssid != NULL || w_pass != NULL || w_bssid != NULL || w_bpass != NULL) {
        err = app_config_save_wifi(w_ssid, w_pass, w_bssid, w_bpass);
    }
    if (err == ESP_OK && have_ip) {
        err = app_config_save_ip(ip_mode, st_ip, st_nm, st_gw, st_dns);
    }
    if (err == ESP_OK && (m_uri != NULL || m_user != NULL || m_pass != NULL)) {
        err = app_config_save_mqtt(m_uri, m_user, m_pass);
    }
    cJSON_Delete(root);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "config save failed: %s", esp_err_to_name(err));
        return send_err_json(req, "500 Internal Server Error", "{\"error\":\"nvs_write_failed\"}");
    }
    ESP_LOGI(TAG, "config saved from portal (takes effect on reboot)");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t h_relay_post(httpd_req_t *req)
{
    if (!check_auth(req)) {
        return ESP_OK;
    }
    char buf[BODY_CAP];
    cJSON *root;
    if (read_json_body(req, buf, sizeof buf, &root) != ESP_OK) {
        return ESP_OK;
    }
    const cJSON *chj = cJSON_GetObjectItemCaseSensitive(root, "channel");
    const char *state = json_str(root, "state");
    const app_config_t *cfg = app_config_get();
    int ch = cJSON_IsNumber(chj) ? chj->valueint : 0;
    bool valid = ch >= 1 && ch <= (int)cfg->relay_count && state != NULL
                 && (strcmp(state, "ON") == 0 || strcmp(state, "OFF") == 0 || strcmp(state, "TOGGLE") == 0);
    if (!valid) {
        cJSON_Delete(root);
        return send_err_json(req, "400 Bad Request", "{\"error\":\"bad_request\"}");
    }
    // Same single-writer path as MQTT/buttons -- works with WiFi/MQTT down.
    // Source is RELAY_SRC_BUTTON: the wire contract only allows
    // "mqtt|button|boot" and the portal is a local control.
    relay_cmd_t cmd = {
        .channel = (uint8_t)ch,
        .op = (strcmp(state, "TOGGLE") == 0) ? RELAY_OP_TOGGLE : RELAY_OP_SET,
        .state = (strcmp(state, "ON") == 0),
        .source = RELAY_SRC_BUTTON,
    };
    bool sent = relay_driver_send_cmd(&cmd);
    cJSON_Delete(root);
    if (!sent) {
        return send_err_json(req, "503 Service Unavailable", "{\"error\":\"queue_full\"}");
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");  // resulting state seen via status polling
}

static esp_err_t h_pass_post(httpd_req_t *req)
{
    if (!check_auth(req)) {
        return ESP_OK;
    }
    char buf[BODY_CAP];
    cJSON *root;
    if (read_json_body(req, buf, sizeof buf, &root) != ESP_OK) {
        return ESP_OK;
    }
    const char *pass = json_str(root, "password");
    if (pass == NULL || pass[0] == '\0' || strlen(pass) > 64) {
        cJSON_Delete(root);
        return send_err_json(req, "400 Bad Request", "{\"error\":\"bad_password\"}");
    }
    esp_err_t err = app_config_save_admin_pass(pass);  // immediate effect; session stays valid
    cJSON_Delete(root);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "admin password save failed: %s", esp_err_to_name(err));
        return send_err_json(req, "500 Internal Server Error", "{\"error\":\"nvs_write_failed\"}");
    }
    ESP_LOGI(TAG, "portal admin password changed");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, app_config_admin_pass_is_default()
                                       ? "{\"ok\":true,\"default_pass\":true}"
                                       : "{\"ok\":true,\"default_pass\":false}");
}

static esp_err_t h_reboot_post(httpd_req_t *req)
{
    if (!check_auth(req)) {
        return ESP_OK;
    }
    drain_body(req);
    ESP_LOGI(TAG, "reboot requested from portal -- restart in 1.5 s");
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, "{\"ok\":true}");
    if (mqtt_mgr_is_connected()) {
        mqtt_mgr_publish("status", "offline", 1, true);  // graceful (mqtt_mgr reboot pattern)
    }
    esp_timer_stop(s_reboot_timer);
    esp_timer_start_once(s_reboot_timer, PORTAL_REBOOT_DELAY_US);  // never esp_restart() in a handler
    return err;
}

// One 302 for every unknown path covers all OS captive probes: /generate_204,
// /gen_204 (Android), /hotspot-detect.html (Apple), /connecttest.txt,
// /ncsi.txt (Windows), /success.txt, /canonical.html (Firefox), favicons, ...
// Deliberately NOT AP-gated (like h_root_get): redirecting a stray LAN client
// to the login page leaks nothing and changes nothing, and gating it would
// break captive-probe behavior for no gain.
static esp_err_t h_404(httpd_req_t *req, httpd_err_code_t error)
{
    (void)error;
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://" AP_IP_STR "/");
    httpd_resp_sendstr(req, "Redirecting to portal");  // iOS wants a body
    return ESP_OK;
}

static const httpd_uri_t s_uris[] = {
    { .uri = "/", .method = HTTP_GET, .handler = h_root_get },
    { .uri = "/api/login", .method = HTTP_POST, .handler = h_login_post },
    { .uri = "/api/logout", .method = HTTP_POST, .handler = h_logout_post },
    { .uri = "/api/status", .method = HTTP_GET, .handler = h_status_get },
    { .uri = "/api/scan", .method = HTTP_GET, .handler = h_scan_get },
    { .uri = "/api/config", .method = HTTP_GET, .handler = h_cfg_get },
    { .uri = "/api/config", .method = HTTP_POST, .handler = h_cfg_post },
    { .uri = "/api/relay", .method = HTTP_POST, .handler = h_relay_post },
    { .uri = "/api/password", .method = HTTP_POST, .handler = h_pass_post },
    { .uri = "/api/reboot", .method = HTTP_POST, .handler = h_reboot_post },
};

// --------------------------------------------------------------- lifecycle --

// Runs in the default event-loop task only.
static void portal_do_start(portal_reason_t reason)
{
    esp_err_t err = wifi_manager_ap_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SoftAP start failed: %s -- portal not started", esp_err_to_name(err));
        return;
    }

    // Captive probes 302 through us constantly; silence per-request noise
    // (captive_portal example pattern).
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.stack_size = PORTAL_HTTPD_STACK;
    hc.max_uri_handlers = 12;
    hc.max_open_sockets = 4;     // + 3 httpd-internal fds; see CONFIG_LWIP_MAX_SOCKETS=16
    hc.lru_purge_enable = true;  // captive mini-browsers abandon sockets freely
    err = httpd_start(&s_httpd, &hc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        wifi_manager_ap_stop();
        return;
    }
    for (size_t i = 0; i < sizeof s_uris / sizeof s_uris[0]; i++) {
        err = httpd_register_uri_handler(s_httpd, &s_uris[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "register %s failed: %s", s_uris[i].uri, esp_err_to_name(err));
            httpd_stop(s_httpd);
            s_httpd = NULL;
            wifi_manager_ap_stop();
            return;
        }
    }
    httpd_register_err_handler(s_httpd, HTTPD_404_NOT_FOUND, h_404);

    // Answer every A query with 192.168.4.1, and bind the listener to it.
    // The literal rather than an if_key ("WIFI_AP_DEF") lookup on purpose: this
    // function runs on the default event-loop task, so the WIFI_EVENT_AP_START
    // that brings the AP netif up cannot be dispatched until we return. An
    // if_key entry resolves here only because esp_netif_get_ip_info falls back
    // to the stored ip_info of a netif that is still down -- a dependency on
    // internal behavior for something we already know statically. AP_IP_ADDR is
    // the same value from the same single source (see its definition).
    dns_server_config_t dc = {
        .num_of_entries = 1,
        .item = { { .name = "*", .if_key = NULL, .ip = { .addr = AP_IP_ADDR } } },
    };
    s_dns = start_dns_server(&dc);
    if (s_dns == NULL) {
        // RULE: the portal is NEVER torn down because the auto-popup helper
        // failed. The DNS hijack is a convenience -- it makes phones open the
        // portal by themselves. Everything that actually recovers the node
        // (login, WiFi/MQTT/static-IP config, relay control, Save & Reboot)
        // works over plain http://192.168.4.1/ without it.
        //
        // Failing the whole start here is a one-way brick: nothing re-posts
        // PORTAL_START_REQ. The unprovisioned request is posted exactly once
        // (wifi_manager_start), the BOOT trigger needs a fresh 5 s hold, and the
        // 180 s downtime timer only re-arms on a disconnect. And the failure is
        // self-inflicted and repeatable: stop_dns_server() deliberately leaks a
        // task still holding a socket bound to 192.168.4.1:53 when it fails to
        // exit in 2 s, and the bind has no SO_REUSEADDR, so the NEXT portal
        // cycle gets ERR_USE -- i.e. the previous cycle would decide the node
        // has no recovery path at all until a power cycle.
        ESP_LOGW(TAG, "captive-portal DNS unavailable - portal reachable at http://" AP_IP_STR
                      "/ but the automatic popup will not appear");
    }

    memset(&s_session, 0, sizeof s_session);  // fresh portal = fresh login
    s_active = true;
    s_reason = reason;
    int32_t r = (int32_t)reason;
    esp_event_post(APP_EVENT, APP_EVENT_PORTAL_STARTED, &r, sizeof r, 0);
    ESP_LOGI(TAG, "portal active (reason=%d) at http://" AP_IP_STR "/ (API is AP-only, DNS hijack %s)",
             (int)reason, s_dns != NULL ? "on" : "OFF");
}

// Runs in the default event-loop task only. Fully tears the portal down:
// DNS + httpd stopped, AP netif freed, session wiped.
static void portal_do_stop(void)
{
    esp_timer_stop(s_grace_timer);
    stop_dns_server(s_dns);
    s_dns = NULL;
    httpd_stop(s_httpd);
    s_httpd = NULL;
    wifi_manager_ap_stop();
    memset(&s_session, 0, sizeof s_session);
    s_active = false;
    esp_event_post(APP_EVENT, APP_EVENT_PORTAL_STOPPED, NULL, 0, 0);
    ESP_LOGI(TAG, "portal stopped (httpd + DNS down, AP netif freed)");
}

static void grace_cb(void *arg)
{
    (void)arg;
    esp_event_post(APP_EVENT, APP_EVENT_PORTAL_STOP_REQ, NULL, 0, 0);
}

static void reboot_cb(void *arg)
{
    (void)arg;
    esp_restart();
}

static void portal_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    switch ((app_event_id_t)id) {
    case APP_EVENT_PORTAL_START_REQ: {
        portal_reason_t reason = (portal_reason_t)*(const int32_t *)data;
        if (s_active) {
            ESP_LOGI(TAG, "start request (reason=%d) ignored -- already active", (int)reason);
        } else {
            portal_do_start(reason);
        }
        break;
    }
    case APP_EVENT_PORTAL_STOP_REQ:
        if (s_active) {
            portal_do_stop();
        }
        break;
    case APP_EVENT_WIFI_GOT_IP:
        // STA recovered while a downtime-triggered portal is up: keep the
        // portal for a grace period, then stop. UNPROVISIONED/MANUAL portals
        // stay up until Save & Reboot.
        if (s_active && s_reason == PORTAL_REASON_STA_DOWN) {
            esp_timer_stop(s_grace_timer);
            esp_timer_start_once(s_grace_timer, PORTAL_GRACE_US);
        }
        break;
    case APP_EVENT_WIFI_LOST:
        if (s_active) {
            esp_timer_stop(s_grace_timer);  // STA dropped during grace -> keep the portal
        }
        break;
    default:
        break;
    }
}

esp_err_t portal_init(void)
{
    const esp_timer_create_args_t gargs = {
        .callback = grace_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "portal_grace",
    };
    esp_err_t err = esp_timer_create(&gargs, &s_grace_timer);
    if (err != ESP_OK) {
        return err;
    }
    const esp_timer_create_args_t rargs = {
        .callback = reboot_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "portal_reboot",
    };
    err = esp_timer_create(&rargs, &s_reboot_timer);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_handler_instance_register(APP_EVENT, ESP_EVENT_ANY_ID, portal_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "ready (servers start on demand)");
    return ESP_OK;
}

bool portal_is_active(void)
{
    return s_active;
}
