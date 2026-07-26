# FOCUS AREA 2 â€” Networking Stack Ground Truth (ESP-IDF v6.0.2, local tree)

Verified against `C:\Espressif\esp\v6.0.2\esp-idf` (`version.txt` = `v6.0.2`). **Not a git checkout** (no `.git`), so this is the distributed release tree.

---

## ðŸ”´ CRITICAL BREAKING CHANGES vs v5.x â€” read first

Two components your v5.x-era spec assumes are **in-tree are gone from IDF v6**:

| Component | v5.x | v6.0.2 | Impact |
|---|---|---|---|
| **esp-mqtt** | `components/mqtt/esp-mqtt/` (git submodule) | **REMOVED** â€” managed component `espressif/mqtt` | Build fails with "mqtt_client.h: No such file" unless added to `idf_component.yml` |
| **cJSON** | `components/json/` (built-in, `REQUIRES json`) | **REMOVED** â€” managed component `espressif/cjson` | `REQUIRES json` is now a hard CMake error |

I verified this exhaustively, not from docs alone:
- `C:\Espressif\esp\v6.0.2\esp-idf\components\mqtt\` contains **only** `test_apps/test_mqtt/sdkconfig.ci.default` and `test_apps/test_mqtt5/sdkconfig.ci.default`. No `CMakeLists.txt`, no `esp-mqtt/`, no sources.
- `C:\Espressif\esp\v6.0.2\esp-idf\components\json\` **does not exist**.
- `.gitmodules` has **zero** entries matching `mqtt` or `json`.
- Filesystem-wide search of `C:\Espressif`: **`mqtt_client.h` and `cJSON.h` do not exist anywhere on this machine.** No component-manager cache in `C:\Users\Izuki\.espressif` either (only `dist`, `espidf.constraints.v6.0.txt`, `idf-env.json`, `python_env`, `tools`).

### âš ï¸ Consequence for item 1 of the task

**I cannot report `esp_mqtt_client_config_t` verbatim â€” the header is not on this machine.** Anything I wrote about exact nesting beyond what appears in in-tree example code would be recall of v5.x, which is exactly what you told me not to do. Below I give (a) every field name provably in use in the v6.0.2 tree, and (b) the exact command to materialize the header for verification.

To obtain ground truth:
```
idf.py add-dependency "espressif/mqtt^1.0.0"
idf.py reconfigure
```
Header then lands at `<project>/managed_components/espressif__mqtt/include/mqtt_client.h`. Docs are now hosted out-of-tree at `https://docs.espressif.com/projects/esp-mqtt/en/latest/`.

---

## 1. esp-mqtt

### Dependency declaration (exact, verbatim from tree)

`C:\Espressif\esp\v6.0.2\esp-idf\examples\protocols\mqtt\main\idf_component.yml`:
```yaml
dependencies:
  protocol_examples_common:
    path: ${IDF_PATH}/examples/common_components/protocol_examples_common
  espressif/mqtt: "^1.0.0"
  espressif/esp_wifi_remote:
    version: ">=0.10,<2.0"
    rules:
      - if: "target in [esp32p4, esp32h2]"
  idf:
    version: ">=5.3"
```
The `esp_wifi_remote` block is **not needed for ESP32-S3** (rule fires only on esp32p4/esp32h2 â€” S3 has native WiFi).

Minimal form for your node, from `examples\mesh\ip_internal_network\main\idf_component.yml`:
```yaml
dependencies:
  espressif/mqtt: "^1.0.0"
```

### Component name for `idf_component_register`

The managed component registers under the **local name `mqtt`**. Confirmed in `examples\mesh\ip_internal_network\main\CMakeLists.txt`:
```cmake
idf_component_register(SRCS "mesh_main.c"
                            "mesh_netif.c"
                            "mqtt_app.c"
                    PRIV_REQUIRES esp_wifi esp-tls nvs_flash mqtt esp_driver_gpio
                    INCLUDE_DIRS "." "include")
```
Note: `examples\protocols\mqtt\main\CMakeLists.txt` **omits** `mqtt` from PRIV_REQUIRES â€” the component manager auto-injects managed deps into the requiring component's requirements. Listing `mqtt` explicitly (as mesh does) also works and is clearer. Do **not** write `esp-mqtt` or `espressif__mqtt` in REQUIRES.

### Config fields provably in use in v6.0.2 (verbatim from example source)

From `examples\protocols\mqtt5\main\app_main.c` lines 224-245 â€” this is the only in-tree v6 sample showing LWT:
```c
const esp_mqtt_client_config_t mqtt5_cfg = {
        .broker = {
            .address.uri = CONFIG_EXAMPLE_MQTT_BROKER_URI,
            .verification.certificate = cert_override_pem,
            .verification.crt_bundle_attach = esp_crt_bundle_attach,
        },
        .session = {
            .protocol_ver = MQTT_PROTOCOL_V_5,
            .last_will = {
                .topic = "/topic/will",
                .msg = "i will leave",
                .msg_len = 12,
                .qos = 1,
                .retain = true,
            },
        },
    };
```

**Confirmed field paths (v6.0.2, in-tree evidence):**
- `.broker.address.uri` â€” also `examples\mesh\ip_internal_network\main\mqtt_app.c:74` uses `.broker.address.uri = "mqtt://mqtt.eclipseprojects.io"`
- `.broker.verification.certificate`
- `.broker.verification.crt_bundle_attach`
- `.session.protocol_ver` (values `MQTT_PROTOCOL_V_5`; 3.1.1 is the default when unset)
- `.session.last_will.topic` / `.msg` / `.msg_len` / `.qos` / `.retain` â† **exactly the LWT fields your spec needs**

**Not provable from this tree** (must verify in the fetched header): `.credentials.username`, `.credentials.authentication.password`, `.credentials.client_id`, `.session.keepalive`, `.session.disable_clean_session`, `.network.*` (`reconnect_timeout_ms`, `timeout_ms`, `disable_auto_reconnect`), `.task.*` (`priority`, `stack_size`, `core_id`), `.buffer.*`. The v5.0 migration guide at `docs\en\migration-guides\release-5.x\5.0\protocols.rst:179-187` documents the grouped-substruct layout introduced in 5.0 and specifically names `esp_mqtt_client_config_t::credentials::username`; the v6.0 migration guide states the API is **unchanged**, only relocated â€” so the v5.x nesting almost certainly carries over. Treat as high-confidence but unverified-locally.

### `esp_mqtt_client_subscribe` â€” macro or function?

**Cannot be determined from this tree** (header absent). Note the two call sites in v6 examples pass exactly 3 args and assign an `int`:
```c
msg_id = esp_mqtt_client_subscribe(client, "topic/qos0", 0);
```
Since IDF v5.x this name has been a variadic/overload-dispatch macro (single-topic vs. topic-list forms). Verify in the fetched header before relying on taking its address or using it in a context that rejects macros.

### Migration guide text (verbatim source of truth in tree)

`docs\en\migration-guides\release-6.x\6.0\protocols.rst:146-153`:
- "The ESP-MQTT component has been removed from ESP-IDF and is now a managed component: `espressif/mqtt`."
- "To add the component to an application, run `idf.py add-dependency espressif/mqtt`."
- "Include headers and APIs remain the same (`mqtt_client.h`), but the component is fetched via the Component Manager."
- Legacy examples under `examples/protocols/mqtt/ssl*` were removed.

`docs\en\api-reference\protocols\mqtt.rst` is now a 40-line stub that only points at the external repo.

---

## 2. Canonical v6 MQTT usage pattern

**âš ï¸ `examples/protocols/mqtt/tcp/main/app_main.c` DOES NOT EXIST in v6.0.2.** The `tcp/`, `ssl/`, `ws/`, `wss/` subdirectories were all deleted. There is now a single flattened example:

`C:\Espressif\esp\v6.0.2\esp-idf\examples\protocols\mqtt\main\app_main.c` (MQTT **over TLS**, cert-bundle by default; there is no plain-TCP example left in IDF â€” TCP examples moved to the `espressif/mqtt` component repo).

### Pattern (verbatim structure from that file)

```c
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED: ...
    }
}

esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
esp_mqtt_client_start(client);
```

`app_main` order: `nvs_flash_init()` â†’ `esp_netif_init()` â†’ `esp_event_loop_create_default()` â†’ network up â†’ `mqtt_app_start()`.

### Event IDs confirmed present in v6.0.2

Handled explicitly in the example: `MQTT_EVENT_CONNECTED`, `MQTT_EVENT_DISCONNECTED`, `MQTT_EVENT_SUBSCRIBED`, `MQTT_EVENT_UNSUBSCRIBED`, `MQTT_EVENT_PUBLISHED`, `MQTT_EVENT_DATA`, `MQTT_EVENT_ERROR`. (Enum also contains `MQTT_EVENT_ANY`/`MQTT_EVENT_BEFORE_CONNECT`/`MQTT_EVENT_DELETED` historically â€” not exercised in-tree, verify in header.)

`esp_mqtt_event_t` fields used verbatim: `event->client`, `event->msg_id`, `event->topic`, `event->topic_len`, `event->data`, `event->data_len`, `event->event_id`, `event->error_handle->error_type`, `->esp_tls_last_esp_err`, `->esp_tls_stack_err`, `->esp_transport_sock_errno`, `->connect_return_code`. Error-type enums: `MQTT_ERROR_TYPE_TCP_TRANSPORT`, `MQTT_ERROR_TYPE_CONNECTION_REFUSED`.

### Does esp-mqtt auto-reconnect?

**Yes.** The example's `MQTT_EVENT_DISCONNECTED` case does nothing but log â€” no restart call â€” which only works because the client reconnects internally. Reconnect is on by default and tuned via the `.network` config group (`disable_auto_reconnect` to turn it off, `reconnect_timeout_ms` for the interval). **Design note for your spec:** implement WiFi-layer exponential backoff yourself, but do **not** hand-roll MQTT reconnect â€” let esp-mqtt own it and just consume CONNECTED/DISCONNECTED events.

### Log tags for `esp_log_level_set` (from the example)
`"mqtt_client"`, `"transport"`, `"transport_base"`, `"outbox"`, `"esp-tls"`.

### Signatures confirmed by call site (arity/return only â€” header absent)
```c
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config);
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client, esp_mqtt_event_id_t event, esp_event_handler_t handler, void *handler_args);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client);
int esp_mqtt_client_publish(client, topic, data, len, qos, retain);   // 6 args, returns msg_id
int esp_mqtt_client_subscribe(client, topic, qos);                    // 3 args, returns msg_id
int esp_mqtt_client_unsubscribe(client, topic);
```
`esp_mqtt_client_enqueue` has **no call site anywhere in the v6.0.2 tree** â€” verify its signature in the fetched header. (It's the non-blocking variant you'll want for publishing from a sensor task without stalling.)

---

## 3. WiFi STA

`C:\Espressif\esp\v6.0.2\esp-idf\examples\wifi\getting_started\station\main\station_example_main.c` â€” **exists and is unchanged in shape from v5.x.** Canonical sequence:

```c
// app_main:
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
  ESP_ERROR_CHECK(nvs_flash_erase());
  ret = nvs_flash_init();
}
ESP_ERROR_CHECK(ret);

// wifi_init_sta:
ESP_ERROR_CHECK(esp_netif_init());
ESP_ERROR_CHECK(esp_event_loop_create_default());
esp_netif_create_default_wifi_sta();
wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
ESP_ERROR_CHECK(esp_wifi_init(&cfg));
esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id);
esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip);
ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
ESP_ERROR_CHECK(esp_wifi_start());
```

Reconnect hook: `WIFI_EVENT_STA_START` â†’ `esp_wifi_connect()`; `WIFI_EVENT_STA_DISCONNECTED` â†’ `esp_wifi_connect()` with a retry counter. **Your backoff logic goes here** â€” the example uses a bare counter (`CONFIG_ESP_MAXIMUM_RETRY`, default 5) with no delay; add an `esp_timer`/FreeRTOS-timer-driven exponential delay rather than blocking in the event handler (it runs on the event-loop task).

### Signatures (verbatim, from headers)
```c
esp_err_t esp_wifi_init(const wifi_init_config_t *config);          // esp_wifi.h:364
esp_err_t esp_wifi_deinit(void);                                    // :376
esp_err_t esp_wifi_set_mode(wifi_mode_t mode);                      // :392
esp_err_t esp_wifi_start(void);                                     // :421
esp_err_t esp_wifi_stop(void);                                      // :434
esp_err_t esp_wifi_connect(void);                                   // :471
esp_err_t esp_wifi_disconnect(void);                                // :482
esp_err_t esp_wifi_set_ps(wifi_ps_type_t type);                     // :680
esp_err_t esp_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf); // :1028
esp_netif_t* esp_netif_create_default_wifi_sta(void);               // esp_wifi_default.h:95
void        esp_netif_destroy_default_wifi(void *esp_netif);        // esp_wifi_default.h:116
esp_err_t esp_event_loop_create_default(void);                      // esp_event.h:68
esp_err_t esp_event_handler_instance_register(esp_event_base_t event_base,
                                              int32_t event_id,
                                              esp_event_handler_t event_handler,
                                              void *event_handler_arg,
                                              esp_event_handler_instance_t *instance); // esp_event.h:245
```

`WIFI_INIT_CONFIG_DEFAULT()` is `esp_wifi.h:316-343`. **v6 additions to `wifi_init_config_t`** not present in older v5.x: `rx_mgmt_buf_type`, `rx_mgmt_buf_num`, `tx_hetb_queue_num`, `dump_hesigb_enable`. Always use the macro; never brace-init the struct by hand.

### Event types (verbatim)
- `ESP_EVENT_DECLARE_BASE(WIFI_EVENT)` â†’ `components\esp_wifi\include\esp_wifi_types_generic.h:1155`
- `WIFI_EVENT_STA_DISCONNECTED` â†’ same file, line 1091
- Disconnect payload, `esp_wifi_types_generic.h:1182-1188`:
```c
typedef struct {
    uint8_t ssid[32];         /**< SSID of disconnected AP */
    uint8_t ssid_len;         /**< SSID length of disconnected AP */
    uint8_t bssid[6];         /**< BSSID of disconnected AP */
    uint8_t reason;           /**< Disconnection reason */
    int8_t  rssi;             /**< Disconnection RSSI */
} wifi_event_sta_disconnected_t;
```
- `ip_event_t` and `IP_EVENT_STA_GOT_IP` â†’ `components\esp_netif\include\esp_netif_types.h:98-116`
- `ip_event_got_ip_t`, `esp_netif_types.h:140-144`:
```c
typedef struct {
    esp_netif_t *esp_netif;          /*!< Pointer to corresponding esp-netif object */
    esp_netif_ip_info_t ip_info;     /*!< IP address, netmask, gateway IP address */
    bool ip_changed;                 /*!< Whether the assigned IP has changed or not */
} ip_event_got_ip_t;
```

### `wifi_sta_config_t` â€” v6 fields you'll touch (`esp_wifi_types_generic.h:553-589`)
```c
uint8_t ssid[32];
uint8_t password[64];
wifi_scan_method_t scan_method;
bool bssid_set;
uint8_t bssid[6];
uint8_t channel;
uint16_t listen_interval;
wifi_sort_method_t sort_method;
wifi_scan_threshold_t  threshold;      // .threshold.authmode
wifi_pmf_config_t pmf_cfg;
uint32_t disable_wpa3_compatible_mode: 1;   // <-- NEW in v6
wifi_sae_pwe_method_t sae_pwe_h2e;
uint8_t failure_retry_cnt;
uint8_t sae_h2e_identifier[SAE_H2E_IDENTIFIER_LEN];  // SAE_H2E_IDENTIFIER_LEN == 32
```

### ðŸ”´ v6 WiFi breaking changes that affect a v5.x-written spec
From `docs\en\migration-guides\release-6.x\6.0\wifi.rst`:
- **`components/esp_wifi/include/esp_interface.h` REMOVED.** `wifi_interface_t` now lives in `esp_wifi_types_generic.h`.
- **`ESP_IF_WIFI_STA` / `ESP_IF_WIFI_AP` macros REMOVED** â€” use `WIFI_IF_STA` / `WIFI_IF_AP`.
- **`WIFI_AUTH_WPA3_EXT_PSK` and `WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE` REMOVED** â€” use `WIFI_AUTH_WPA3_PSK`.
- **`WIFI_BW_HT20` / `WIFI_BW_HT40` REMOVED** â€” use `WIFI_BW20` / `WIFI_BW40`.
- **Disconnect reasons `WIFI_REASON_ASSOC_EXPIRE`, `WIFI_REASON_NOT_AUTHED`, `WIFI_REASON_NOT_ASSOCED` REMOVED** â†’ `WIFI_REASON_AUTH_EXPIRE`, `WIFI_REASON_CLASS2_FRAME_FROM_NONAUTH_STA`, `WIFI_REASON_CLASS3_FRAME_FROM_NONASSOC_STA`. **Relevant to your backoff logic** if you branch on reason codes.
- **`esp_wifi_init()` called twice now returns `ESP_ERR_INVALID_STATE`, not `ESP_OK`.** If your init is idempotent-by-retry, `ESP_ERROR_CHECK` will now abort.
- Antenna APIs `esp_wifi_set_ant*` removed â†’ moved to `esp_phy`.

### REQUIRES for WiFi
`examples\wifi\getting_started\station\main\CMakeLists.txt`:
```cmake
idf_component_register(SRCS "station_example_main.c"
                    PRIV_REQUIRES esp_wifi nvs_flash
                    INCLUDE_DIRS ".")
```
`esp_event` and `esp_netif` are transitive via `esp_wifi`, but list them explicitly if you `#include "esp_event.h"` / `"esp_netif.h"` directly. All three component names â€” `esp_wifi`, `esp_netif`, `esp_event` â€” are unchanged and confirmed present in `components/`.

---

## 4. MAC address for `node_id`

`C:\Espressif\esp\v6.0.2\esp-idf\components\esp_hw_support\include\esp_mac.h` â€” **unchanged from v5.x.**

```c
typedef enum {
    ESP_MAC_WIFI_STA,      /**< MAC for WiFi Station (6 bytes) */
    ESP_MAC_WIFI_SOFTAP,   /**< MAC for WiFi Soft-AP (6 bytes) */
    ESP_MAC_BT,            /**< MAC for Bluetooth (6 bytes) */
    ESP_MAC_ETH,           /**< MAC for Ethernet (6 bytes) */
    ESP_MAC_IEEE802154,    /**< if CONFIG_SOC_IEEE802154_SUPPORTED=y, MAC for IEEE802154 (8 bytes) */
    ESP_MAC_BASE,          /**< Base MAC for that used for other MAC types (6 bytes) */
    ESP_MAC_EFUSE_FACTORY, /**< MAC_FACTORY eFuse which was burned by Espressif in production (6 bytes) */
    ESP_MAC_EFUSE_CUSTOM,  /**< MAC_CUSTOM eFuse which was can be burned by customer (6 bytes) */
    ESP_MAC_EFUSE_EXT,     /**< if CONFIG_SOC_IEEE802154_SUPPORTED=y, MAC_EXT eFuse ... (2 bytes) */
} esp_mac_type_t;
```
(lines 21-31)

```c
esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type);   // :129
esp_err_t esp_efuse_mac_get_default(uint8_t *mac);           // :111
esp_err_t esp_base_mac_addr_get(uint8_t *mac);               // :75
esp_err_t esp_base_mac_addr_set(const uint8_t *mac);         // :61
esp_err_t esp_iface_mac_addr_set(const uint8_t *mac, esp_mac_type_t type);  // :161
size_t    esp_mac_addr_len_get(esp_mac_type_t type);         // :178
```

Free helper macros at lines 12-15 â€” use these for your `node_id` string:
```c
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
```

**Recommendation:** `esp_read_mac(mac, ESP_MAC_WIFI_STA)` â€” 6 bytes, works before `esp_wifi_init()`, stable across reboots. `esp_efuse_mac_get_default()` returns the *base* MAC (factory eFuse), which on S3 equals the STA MAC but is semantically the base, not the interface. Component: `esp_hw_support` (usually transitive via `esp_common`/`esp_system`; add to REQUIRES if the include fails).

---

## 5. JSON â€” cJSON moved out

**Confirmed: `components/json` does not exist in v6.0.2.** No `cJSON.h` anywhere under `C:\Espressif`.

### Exact dependency line used by v6 examples

Three in-tree manifests, all identical version constraint:
```yaml
espressif/cjson: "^1.7.19"
```
- `examples\protocols\http_server\restful_server\main\idf_component.yml`
- `examples\peripherals\i2c\i2c_slave_network_sensor\main\idf_component.yml` â† **closest analogue to your SHT31-over-I2C + JSON node**
- `components\esp_tee\test_apps\tee_test_fw\main\idf_component.yml`

Full example (`restful_server`):
```yaml
## IDF Component Manager Manifest File
dependencies:
  espressif/cjson: "^1.7.19"
  espressif/mdns: "^1.8.0"
  joltwallet/littlefs: "^1.20.0"
  protocol_examples_common:
    path: ${IDF_PATH}/examples/common_components/protocol_examples_common
```

### REQUIRES for cJSON

**Do not add anything.** Grepping every `CMakeLists.txt` under `examples/` and `components/` for `cjson`/`cJSON` returns **zero hits** â€” the component manager injects it. Verbatim from `docs\en\migration-guides\release-6.x\6.0\protocols.rst:19-29`:
```cmake
# Before
idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES json esp_http_server)

# After
idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES esp_http_server)
```

### API unchanged
Migration guide, step 3: "No Code Changes Required." `#include "cJSON.h"`, `cJSON_Parse`, `cJSON_GetObjectItem`, `cJSON_Delete` all work as before. Upstream is now `https://github.com/espressif/idf-extra-components/tree/master/cjson`.

---

## Recommended manifest + CMake for your node

`main/idf_component.yml`:
```yaml
dependencies:
  idf:
    version: ">=6.0"
  espressif/mqtt: "^1.0.0"
  espressif/cjson: "^1.7.19"
```

`main/CMakeLists.txt`:
```cmake
idf_component_register(SRCS "app_main.c" ...
    PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_timer
                  esp_driver_gpio esp_driver_i2c esp_hw_support mqtt
    INCLUDE_DIRS ".")
```
(`mqtt` is optional-but-explicit per the mesh example; `cjson` must be omitted. Note `esp_driver_i2c` / `esp_driver_gpio` â€” the monolithic `driver` component is legacy in v6; that's Focus Area 1's territory but it affects this file.)

---

## Open items requiring the fetched header

After `idf.py add-dependency "espressif/mqtt^1.0.0" && idf.py reconfigure`, read `managed_components/espressif__mqtt/include/mqtt_client.h` and confirm:
1. Exact nesting of `.credentials.username`, `.credentials.authentication.password`, `.credentials.client_id`
2. `.session.keepalive`, `.session.disable_clean_session`
3. `.network.reconnect_timeout_ms`, `.network.timeout_ms`, `.network.disable_auto_reconnect`
4. `.task.priority`, `.task.stack_size`
5. Whether `esp_mqtt_client_subscribe` is a macro or plain function
6. `esp_mqtt_client_enqueue` signature (the `store` bool parameter)
7. Full `esp_mqtt_event_id_t` enumerator list
8. Component Kconfig names â€” the `CONFIG_MQTT_*` symbols now come from the managed component, not IDF, so `CONFIG_MQTT_*` availability differs. The only MQTT Kconfig symbols left in-tree are the test-app ones (`CONFIG_MQTT_TEST_BROKER_URI`, `CONFIG_MQTT5_TEST_BROKER_URI`), which are example-local, not library options.