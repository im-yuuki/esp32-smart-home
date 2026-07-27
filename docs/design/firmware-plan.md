All three reports are digested, and I additionally verified `mqtt_client.h` (v1.0.0) byte-for-byte from the offline mirror zip at `C:\Espressif\tools\components\espressif\mqtt\1.0.0\espressif__mqtt-v1.0.0.zip` â€” so the MQTT config struct fields the reports flagged as "unverified" are now confirmed ground truth. The plan below is fully grounded.

# ESP32-S3 Firmware Implementation Plan â€” Phase 1 Smart-Home Node

Target: ESP-IDF **v6.0.2** (local tree `C:\Espressif\esp\v6.0.2\esp-idf`), target `esp32s3`, project root `C:\Users\Izuki\Projects\SmartHomeController\firmware\`.

> Repo-layout note: the project root currently contains `esp32/`, `esp32-webui/`, `server/`, `server-webui/` (all empty) and **no** `firmware/`. This plan follows the roadmap-prescribed `firmware/` path. If the executor is told `esp32/` is the intended firmware dir, substitute the path â€” nothing else changes.

## 0. Verified ground-truth summary (decisions locked by evidence)

| Fact | Consequence |
|---|---|
| esp-mqtt removed from IDF v6 â†’ managed component `espressif/mqtt` ^1.0.0; **present in offline mirror** (`C:\Espressif\tools\components\espressif\mqtt\1.0.0`) | Declare in `main/idf_component.yml`; resolves offline. Never put `mqtt` in a REQUIRES of a *missing* in-tree component â€” but listing `mqtt` in `PRIV_REQUIRES` is valid (mesh example pattern). |
| cJSON removed â†’ `espressif/cjson` ^1.7.19; mirror has `1.7.19~2` | Declare in manifest; **never** list `json`/`cjson` in REQUIRES (component manager injects it into `main`). |
| `esp_mqtt_client_config_t` verified from zip: `.broker.address.uri`, `.credentials.username`, `.credentials.client_id`, `.credentials.authentication.password`, `.session.last_will.{topic,msg,msg_len,qos,retain}`, `.session.keepalive`, `.session.disable_clean_session`, `.network.{reconnect_timeout_ms,timeout_ms,disable_auto_reconnect}`, `.task.{priority,stack_size}`, `.buffer.{size,out_size}`, `.outbox.limit` | Config code below is exact, no longer "verify at first build". |
| `esp_mqtt_client_subscribe` is a `_Generic` **macro** â†’ call **`esp_mqtt_client_subscribe_single(client, topic, qos)`** directly | Avoids `_Generic` surprises with array-typed literals. |
| `esp_mqtt_client_enqueue(client, topic, data, len, qos, retain, store)` â€” 7 args, non-blocking, outbox-backed | This is our "publish queue"; do not build a custom one. |
| esp-mqtt auto-reconnects by default (`.network.reconnect_timeout_ms`, default 10 s) | Only WiFi backoff is hand-rolled; MQTT reconnect belongs to esp-mqtt. `esp_mqtt_client_reconnect()` exists to shortcut after IP re-acquired. |
| GPIO: component `esp_driver_gpio`, header `driver/gpio.h`; I2C: `esp_driver_i2c`, `driver/i2c_master.h`; I2C NACK now returns **`ESP_ERR_INVALID_RESPONSE`**; `i2c_master_probe` returns `ESP_ERR_NOT_FOUND` on NACK, `ESP_ERR_TIMEOUT` w/o pull-ups | Sensor-absent detection keys off these codes. |
| Driver/`esp_event.h` headers no longer include FreeRTOS headers; task stack sizes are in **BYTES**; `vTaskDelayUntil` removed â†’ `xTaskDelayUntil` | Explicit includes everywhere; numbers below are bytes. |
| Default warnings are errors (GCC 15.2.0); escape hatches `CONFIG_COMPILER_DISABLE_GCC15_WARNINGS`, `CONFIG_COMPILER_DISABLE_DEFAULT_ERRORS` | See sdkconfig.defaults Â§3. |
| `CONFIG_ESPTOOLPY_FLASHSIZE` defaults to 2 MB; no auto-sizing partition CSV | Custom `partitions.csv` + explicit flash size. |
| Version: `set(PROJECT_VER "1.0.0")` before `include(project.cmake)`; runtime read via `esp_app_get_description()` from `esp_app_desc.h` (component `esp_app_format`; `esp_ota_get_app_description` is **removed**) | Discovery `fw_version` comes from `esp_app_get_description()->version`. |
| Build activation: dot-source `C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1` (sets `IDF_COMPONENT_LOCAL_STORAGE_URL=file://C:\Espressif\tools`) | **Scripted builds must set `IDF_COMPONENT_LOCAL_STORAGE_URL` manually** or dependency resolution will try the network. |

Remaining "verify at first build" items (all low-risk, defensively designed):
1. Managed-component Kconfig names `CONFIG_MQTT_TASK_STACK_SIZE` etc. â€” we sidestep by setting `.task.stack_size`/`.buffer.size` explicitly in the config struct.
2. `esp_netif_sntp.h` location/API (SNTP for the sensor `ts` field) â€” wrapped in one function; if it fails to compile, fallback is documented (Â§2.9).
3. `%f` printf support (picolibc default) â€” sensor JSON uses `%.1f`; if nano-format strips floats, add `CONFIG_LIBC_NEWLIB_NANO_FORMAT=n`. Test in step 2.

## 1. Final file tree

```
firmware/
â”œâ”€â”€ CMakeLists.txt              # root: cmake 3.22, PROJECT_VER, project()
â”œâ”€â”€ partitions.csv              # custom table (nvs 24K, factory 3M)
â”œâ”€â”€ sdkconfig.defaults          # committed; exact content in Â§3
â”œâ”€â”€ .gitignore                  # sdkconfig, sdkconfig.old, build/, managed_components/
â”œâ”€â”€ dependencies.lock           # generated on first reconfigure â€” COMMIT it (pins mirror versions)
â””â”€â”€ main/
    â”œâ”€â”€ CMakeLists.txt
    â”œâ”€â”€ idf_component.yml       # espressif/mqtt ^1.0.0, espressif/cjson ^1.7.19
    â”œâ”€â”€ Kconfig.projbuild       # provisioning defaults (wifi/mqtt/room/gpio/â€¦)
    â”œâ”€â”€ main.c                  # app_main: sequential init, then returns
    â”œâ”€â”€ app_events.h            # ESP_EVENT_DECLARE_BASE(APP_EVENT) + event IDs
    â”œâ”€â”€ app_events.c            # ESP_EVENT_DEFINE_BASE(APP_EVENT)
    â”œâ”€â”€ app_config.h / app_config.c      # NVS schema, first-boot seeding, node_id
    â”œâ”€â”€ wifi_manager.h / wifi_manager.c  # STA + exponential backoff via esp_timer
    â”œâ”€â”€ mqtt_mgr.h / mqtt_mgr.c          # RENAMED from mqtt_client.* (see below)
    â”œâ”€â”€ relay_driver.h / relay_driver.c  # relay task + cmd queue + NVS persist
    â”œâ”€â”€ button_handler.h / button_handler.c  # ISR -> esp_timer 50ms -> queue
    â”œâ”€â”€ sensor_driver.h         # generic sensor vtable (SHT31 today, DHT22 later)
    â”œâ”€â”€ sht31.h / sht31.c       # SHT31 impl of sensor_driver over i2c_master
    â”œâ”€â”€ sensor_task.h / sensor_task.c    # probe-at-boot, periodic read+publish
    â””â”€â”€ discovery.h / discovery.c        # cJSON discovery payload builder
```

**Rename decision (`mqtt_client.c/.h` â†’ `mqtt_mgr.c/.h`), justification:** the managed component's public header is literally `mqtt_client.h`, and `main` is registered with `INCLUDE_DIRS "."` â€” a local `main/mqtt_client.h` would shadow the component header for every `#include "mqtt_client.h"` in our sources (include-guard/type collisions, broken IntelliSense, and our wrapper could not include the real API by that name). Renaming the module is the only clean fix; the roadmap explicitly allows it with justification.

## 2. Per-module design

### 2.0 Task / queue / event architecture (system view)

| Task | Created by | Priority | Stack (bytes) | Core | Blocks on | TWDT |
|---|---|---|---|---|---|---|
| `main` (runs `app_main`) | IDF | 1 | 4096 (`CONFIG_ESP_MAIN_TASK_STACK_SIZE`) | 0 | â€” (returns after init) | no |
| `relay_task` | relay_driver | **10** | 4096 | any (`tskNO_AFFINITY`) | `xQueueReceive(relay_cmd_q, 2 s timeout)` | **yes** |
| `sensor_task` | sensor_task (only if sensor present) | 4 | 4096 | any | 1 s `vTaskDelay` slices | **yes** |
| `mqtt_task` | esp-mqtt internally | 5 (`.task.priority`) | 6144 (`.task.stack_size`) | â€” | internal | no (owned by lib) |
| `sys_evt` (default event loop) | esp_event | 20 | 4096 (`CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096`, raised from 2304 â€” our handlers call cJSON + NVS) | 0 | internal | no |
| `esp_timer` | IDF | 22 | 3584 default | 0 | internal | no |
| `wifi` | esp_wifi | 23 | â€” | 0 | internal | no |

Communication paths:
- **`relay_cmd_q`** (`QueueHandle_t`, length 8, item `relay_cmd_t`, 8 bytes): the ONLY writer-side for relay changes. Producers: MQTT `MQTT_EVENT_DATA` handler (mqtt_task context), button debounce callback (esp_timer task context), boot restore (main task). Consumer: `relay_task`.
- **`APP_EVENT` on the default esp_event loop** (`app_events.h`): `APP_EVENT_WIFI_GOT_IP`, `APP_EVENT_WIFI_LOST`. Producer: wifi_manager. Consumer: mqtt_mgr (start client once / `esp_mqtt_client_reconnect()`).
- MQTT publishes: only via `mqtt_mgr_publish()` â†’ `esp_mqtt_client_enqueue(..., store=true)` â€” never blocks, never from ISR (producers above are all task contexts; the spec's "no MQTT publish from ISR" is satisfied structurally: ISRs only touch `esp_timer`).

TWDT: rely on default auto-init (`CONFIG_ESP_TASK_WDT_INIT=y`); do **not** call `esp_task_wdt_init()` again (returns `ESP_ERR_INVALID_STATE`). `relay_task` and `sensor_task` call `esp_task_wdt_add(NULL)` at entry and `esp_task_wdt_reset()` every loop iteration; both loops are bounded â‰¤2 s per iteration, well under the 10 s timeout. `CONFIG_ESP_TASK_WDT_PANIC=y` makes a hung task reboot the node.

### 2.1 `main.c`

`app_main()` sequential init â€” exact order matters (sensor probe must precede first discovery publish; relays must restore before network so local control never waits on WiFi):

```c
1. NVS:  nvs_flash_init(); if ESP_ERR_NVS_NO_FREE_PAGES || ESP_ERR_NVS_NEW_VERSION_FOUND â†’ nvs_flash_erase(); retry.
2. app_config_init();            // seed-from-Kconfig on first boot, load config, derive node_id
3. esp_event_loop_create_default();
4. relay_driver_init();          // GPIO out, power-on behavior applied HERE (source "boot"), task+queue created
5. button_handler_init();        // GPIO in + ISR service + debounce timers
6. sensor_task_start();          // I2C bus + probe; absent â†’ warn + no task
7. wifi_manager_start();         // netif, esp_wifi, handlers, esp_wifi_start()
8. mqtt_mgr_start();             // builds client config, registers APP_EVENT handler; client started on first GOT_IP
9. return;                       // main task ends; system runs on tasks/events
```

Every step logs under tag `"MAIN"`; failures in 4â€“6 are logged but non-fatal (a node with a broken sensor must still serve relays); failures in 1â€“3 are fatal (`ESP_ERROR_CHECK`).

Includes pitfall (applies to **every** module): explicitly `#include "freertos/FreeRTOS.h"`, `"freertos/task.h"`, `"freertos/queue.h"` as needed â€” v6 driver headers and `esp_event.h` no longer pull them in.

### 2.2 `app_config` â€” NVS schema + node_id

Public API:
```c
typedef enum { POWERON_RESTORE = 0, POWERON_OFF = 1, POWERON_ON = 2 } poweron_behavior_t;

typedef struct {
    char     wifi_ssid[33], wifi_pass[65];
    char     mqtt_uri[128], mqtt_user[33], mqtt_pass[65];
    char     room[33];                       // slug, e.g. "livingroom"
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

esp_err_t          app_config_init(void);
const app_config_t *app_config_get(void);
esp_err_t          app_config_save_relay_state(uint8_t ch, bool on);   // ch is 1-based
esp_err_t          app_config_load_relay_state(uint8_t ch, bool *on);  // ESP_ERR_NVS_NOT_FOUND if never saved
```

NVS layout (all key names â‰¤15 chars â€” hard NVS limit):

| Namespace | Key | Type | Default (from Kconfig.projbuild) |
|---|---|---|---|
| `shc_cfg` | `cfg_ver` | u8 | 1 (schema version; absence â‡’ first boot â‡’ seed all keys from `CONFIG_SHC_*` and `nvs_commit`) |
| `shc_cfg` | `wifi_ssid` / `wifi_pass` | str | `CONFIG_SHC_WIFI_SSID` / `_PASS` |
| `shc_cfg` | `mqtt_uri` | str | `CONFIG_SHC_MQTT_URI` = `"mqtt://192.168.1.10:1883"` |
| `shc_cfg` | `mqtt_user` / `mqtt_pass` | str | `CONFIG_SHC_MQTT_USER` / `_PASS` |
| `shc_cfg` | `room` | str | `CONFIG_SHC_ROOM` = `"livingroom"` |
| `shc_cfg` | `relay_cnt` | u8 | 2 |
| `shc_cfg` | `r1_gpio` / `r2_gpio` | u8 | 4 / 5 (`CONFIG_SHC_RELAY1_GPIO`/`2`) |
| `shc_cfg` | `r1_name` / `r2_name` | str | `"Den tran"` / `"Den ban"` |
| `shc_cfg` | `relay_act` | u8 | 1 (`CONFIG_SHC_RELAY_ACTIVE_HIGH` bool â†’ 1/0) |
| `shc_cfg` | `poweron` | u8 | 0=restore (choice `CONFIG_SHC_POWERON_*`) |
| `shc_cfg` | `btn1_gpio` / `btn2_gpio` | u8 | 6 / 7 |
| `shc_cfg` | `i2c_sda` / `i2c_scl` | u8 | 8 / 9 |
| `shc_cfg` | `sens_int_s` | u32 | 30 (`CONFIG_SHC_SENSOR_INTERVAL_S`, range 5â€“3600) |
| `shc_state` | `r1_state` / `r2_state` | u8 | written on every relay change (`nvs_set_u8` + `nvs_commit`); read by power-on restore |

String reads use the confirmed two-call `nvs_get_str(h, key, NULL, &len)` pattern, but into the fixed buffers above (pass buffer size, check `ESP_ERR_NVS_INVALID_LENGTH`). Config and state use **separate namespaces** so a future "factory reset config" (`nvs_erase_all` on `shc_cfg`) won't clobber relay state, and vice versa.

`node_id`: `esp_read_mac(mac, ESP_MAC_WIFI_STA)` (works before `esp_wifi_init`, component `esp_hw_support`), then `snprintf(node_id, sizeof node_id, "esp32s3-%02x%02x%02x", mac[3], mac[4], mac[5])` â€” lowercase hex, last 3 bytes = last 6 hex digits.

`Kconfig.projbuild` (esp-idf-kconfig **v3** â€” keep to plain `menu`/`config`/`choice` with `string`/`int`/`bool`, `default`, `range`, `help`; no exotic constructs):

```
menu "Smart Home Node Configuration"
    config SHC_WIFI_SSID
        string "WiFi SSID (seeded to NVS on first boot)"
        default "myssid"
    config SHC_WIFI_PASS
        string "WiFi password"
        default "mypassword"
    config SHC_MQTT_URI
        string "MQTT broker URI"
        default "mqtt://192.168.1.10:1883"
    config SHC_MQTT_USER
        string "MQTT username"
        default "smarthome"
    config SHC_MQTT_PASS
        string "MQTT password"
        default "changeme"
    config SHC_ROOM
        string "Room slug"
        default "livingroom"
    config SHC_RELAY1_GPIO
        int "Relay 1 GPIO"
        range 0 48
        default 4
    config SHC_RELAY2_GPIO
        int "Relay 2 GPIO"
        range 0 48
        default 5
    config SHC_RELAY1_NAME
        string "Relay 1 name"
        default "Den tran"
    config SHC_RELAY2_NAME
        string "Relay 2 name"
        default "Den ban"
    config SHC_RELAY_ACTIVE_HIGH
        bool "Relays are active-high"
        default y
    choice SHC_POWERON_BEHAVIOR
        prompt "Relay power-on behavior"
        default SHC_POWERON_RESTORE
        config SHC_POWERON_RESTORE
            bool "Restore last state from NVS"
        config SHC_POWERON_OFF
            bool "Always off"
        config SHC_POWERON_ON
            bool "Always on"
    endchoice
    config SHC_BUTTON1_GPIO
        int "Button 1 GPIO"
        default 6
    config SHC_BUTTON2_GPIO
        int "Button 2 GPIO"
        default 7
    config SHC_I2C_SDA_GPIO
        int "I2C SDA GPIO (SHT31)"
        default 8
    config SHC_I2C_SCL_GPIO
        int "I2C SCL GPIO (SHT31)"
        default 9
    config SHC_SENSOR_INTERVAL_S
        int "Sensor publish interval (seconds)"
        range 5 3600
        default 30
endmenu
```

Provisioning bootstrap semantics: Kconfig values are compiled-in **defaults only**; `app_config_init()` copies them into NVS **once** (when `cfg_ver` is absent). Afterwards NVS is the single source of truth â€” reflashing with different menuconfig values does *not* overwrite an existing config (that's what makes later runtime provisioning possible). To force re-seed during development: `idf.py -p COM4 erase-flash`.

### 2.3 `relay_driver` â€” actuation path for all sources

```c
typedef enum { RELAY_SRC_MQTT, RELAY_SRC_BUTTON, RELAY_SRC_BOOT } relay_source_t;
typedef enum { RELAY_OP_SET, RELAY_OP_TOGGLE } relay_op_t;
typedef struct {
    uint8_t        channel;   // 1-based
    relay_op_t     op;
    bool           state;     // used when op == RELAY_OP_SET
    relay_source_t source;
} relay_cmd_t;

esp_err_t relay_driver_init(void);                              // gpio_config outputs, power-on behavior, queue+task
bool      relay_driver_send_cmd(const relay_cmd_t *cmd);        // xQueueSend, 0 ticks; task context only
bool      relay_driver_get_state(uint8_t ch);                   // cached, for discovery/connect-sequence
```

Init: `gpio_config()` with `pin_bit_mask = (1ULL<<r1)|(1ULL<<r2)`, `GPIO_MODE_OUTPUT`, no pulls, `GPIO_INTR_DISABLE`. Immediately drive both to inactive level (`!active_level`) **before** applying power-on behavior, so relays never glitch on. Then compute the boot state per `poweron` (`restore` â†’ `app_config_load_relay_state`, not-found â‡’ off) and enqueue `RELAY_OP_SET` commands with `RELAY_SRC_BOOT` â€” they flow through the same path as everything else.

`relay_task` loop (the **only** place a relay GPIO is written â€” serializes button vs MQTT races by construction):
```
esp_task_wdt_add(NULL);
for (;;) {
    if (xQueueReceive(q, &cmd, pdMS_TO_TICKS(2000)) == pdTRUE) {
        bool new_state = (cmd.op == RELAY_OP_TOGGLE) ? !cached[ch] : cmd.state;
        gpio_set_level(gpio[ch], new_state ? active : !active);      // 1. ACT
        cached[ch] = new_state;
        app_config_save_relay_state(ch, new_state);                  // 2. PERSIST (nvs_commit)
        mqtt_mgr_publish_relay_state(ch, new_state, cmd.source);     // 3. REPORT (no-op if offline)
    }
    esp_task_wdt_reset();
}
```
This enforces the invariant **command â†’ act â†’ report**: publish happens strictly after `gpio_set_level`. With WiFi/MQTT down, steps 1â€“2 still run (button works offline); step 3 is skipped when disconnected, and resync is guaranteed because the MQTT connect sequence republishes all cached states (Â§2.5). Priority 10 > mqtt task 5 keeps local latency low.

### 2.4 `button_handler` â€” ISR â†’ esp_timer â†’ queue path

```c
esp_err_t button_handler_init(void);
```

- `gpio_config()`: inputs, `GPIO_PULLUP_ENABLE`, `GPIO_INTR_NEGEDGE` (press = falling edge with pull-up).
- `gpio_install_isr_service(0)` â€” **no** `ESP_INTR_FLAG_IRAM`, so handlers need no `IRAM_ATTR` (verified doc note) and calling esp_timer start functions is unrestricted. `gpio_isr_handler_add(pin, isr, (void*)channel)` per button. (Never mix with `gpio_isr_register` â€” mutually exclusive.)
- One **one-shot** `esp_timer` per button (`ESP_TIMER_TASK` dispatch), created at init.
- ISR body (complete): `esp_timer_stop(t[ch]); esp_timer_start_once(t[ch], 50000);` â€” the stop-then-start pattern makes the 50 ms window retriggerable across bounces (`esp_timer_start_once` on a running timer returns `ESP_ERR_INVALID_STATE`; `esp_timer_restart` requires it running â€” stop+start covers both). Both are IRAM-safe out of the box (`CONFIG_ESP_TIMER_IN_IRAM` default y). **No delay, no queue, no logging, no MQTT in the ISR.**
- Timer callback (runs in the esp_timer task, normal task context): sample `gpio_get_level(pin)`; if still 0 (pressed after 50 ms â‡’ real press), `relay_driver_send_cmd(&(relay_cmd_t){ .channel = ch, .op = RELAY_OP_TOGGLE, .source = RELAY_SRC_BUTTON })`. Plain `xQueueSend`, not the FromISR variant.

Full path: press â†’ GPIO ISR (Âµs) â†’ 50 ms one-shot â†’ callback confirms level â†’ `relay_cmd_q` â†’ `relay_task` actuates â†’ publishes `{"state":...,"source":"button"}`. Works with WiFi down end-to-end (nothing in the path touches the network until step 3 of relay_task, which is skip-on-offline).

### 2.5 `mqtt_mgr` â€” esp-mqtt wrapper (LWT, connect sequence, publish)

```c
esp_err_t mqtt_mgr_start(void);   // build config from app_config, init client, register handlers; started on first APP_EVENT_WIFI_GOT_IP
bool      mqtt_mgr_is_connected(void);
int       mqtt_mgr_publish(const char *subtopic, const char *payload, int qos, bool retain);  // topic = base + subtopic
void      mqtt_mgr_publish_relay_state(uint8_t ch, bool state, relay_source_t src);
```

Base topic built once at start: `snprintf(base, ..., "home/%s/%s/", cfg->room, cfg->node_id)`.

Client config (every field verified in the mirror header):
```c
esp_mqtt_client_config_t mc = {
    .broker.address.uri = cfg->mqtt_uri,
    .credentials.username = cfg->mqtt_user,
    .credentials.client_id = cfg->node_id,
    .credentials.authentication.password = cfg->mqtt_pass,
    .session.last_will = { .topic = status_topic /* "home/{room}/{id}/status" */,
                           .msg = "offline", .msg_len = 0, .qos = 1, .retain = 1 },
    .session.keepalive = 30,                 // LWT fires â‰¤ ~45 s after ungraceful death â†’ meets â‰¤90 s criterion
    .network.reconnect_timeout_ms = 5000,    // esp-mqtt owns MQTT-level reconnect
    .network.timeout_ms = 10000,
    .task = { .priority = 5, .stack_size = 6144 },   // explicit â†’ independent of managed-component Kconfig defaults
    .buffer = { .size = 2048 },              // discovery JSON â‰ˆ 400 B; headroom
    .outbox.limit = 8192,                    // bound RAM if broker unreachable while QoS1 msgs queue
};
```

Lifecycle & state handling:
- `mqtt_mgr_start()` creates the client (`esp_mqtt_client_init`), registers `esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, handler, NULL)`, and registers an `APP_EVENT` handler on the default loop. It does **not** start the client yet.
- On `APP_EVENT_WIFI_GOT_IP`: first time â†’ `esp_mqtt_client_start()`; subsequently â†’ `esp_mqtt_client_reconnect()` (shortcut esp-mqtt's retry wait after WiFi came back).
- `MQTT_EVENT_CONNECTED` (runs in mqtt task) â€” the spec's exact connect sequence, in order:
  1. publish `status` = `"online"`, QoS 1, retained;
  2. `discovery_publish()` (QoS 1, retained);
  3. for each channel: publish `relay/{ch}/state` from `relay_driver_get_state()`, retained (this is the offline-resync mechanism â€” source `"boot"` on first connect after boot, `"mqtt"`-refresh uses last-known source; simplest correct choice: remember last source per channel in relay_driver and republish with it);
  4. `esp_mqtt_client_subscribe_single(client, base+"relay/+/set", 1)` and `..."cmd", 1)`.
  Sets `connected = true` (atomic bool).
- `MQTT_EVENT_DISCONNECTED`: `connected = false`, log only â€” **no manual restart** (esp-mqtt reconnects itself; verified behavior).
- `MQTT_EVENT_DATA`: guard fragmented messages (`event->total_data_len == event->data_len`, else log+drop â€” our payloads are tiny). Topic match on `event->topic/topic_len` (not NUL-terminated!):
  - `relay/{ch}/set`: parse `ch`, `cJSON_ParseWithLength(event->data, event->data_len)`, read `"state"` == `"ON"|"OFF"`, enqueue `{ch, RELAY_OP_SET, state, RELAY_SRC_MQTT}`. Invalid payload â†’ `ESP_LOGW`, drop.
  - `cmd`: parse JSON; `"action":"reboot"` â†’ publish `status` `"offline"` retained (graceful), then arm a one-shot `esp_timer` (1500 ms) whose callback calls `esp_restart()` â€” never reboot inside the event handler, and the delay lets the publish flush. Unknown action â†’ `ESP_LOGW`.
- `MQTT_EVENT_ERROR`: log `error_type`, `connect_return_code` (e.g. `MQTT_CONNECTION_REFUSE_BAD_USERNAME`), `esp_transport_sock_errno` â€” makes bad-credential debugging trivial.
- `mqtt_mgr_publish()`: if `!connected && qos == 0` return âˆ’1 (don't grow outbox with stale sensor data); else `esp_mqtt_client_enqueue(client, topic, payload, 0, qos, retain, true)` â€” non-blocking for all callers (relay_task, sensor_task). Inside the CONNECTED handler, plain `esp_mqtt_client_publish` is also fine (in-tree examples do it), but using the same enqueue path everywhere is simpler.

### 2.6 `wifi_manager` â€” STA + exponential backoff

```c
esp_err_t   wifi_manager_start(void);
bool        wifi_manager_is_connected(void);
const char *wifi_manager_get_ip_str(void);   // "192.168.1.57", cached from ip_event_got_ip_t
```

Canonical v6 sequence (verified example): `esp_netif_init()` â†’ `esp_netif_create_default_wifi_sta()` â†’ `WIFI_INIT_CONFIG_DEFAULT()` (**always the macro** â€” v6 added fields) â†’ `esp_wifi_init()` â†’ `esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, ...)` + `(IP_EVENT, IP_EVENT_STA_GOT_IP, ...)` â†’ `esp_wifi_set_mode(WIFI_MODE_STA)` â†’ `esp_wifi_set_config(WIFI_IF_STA, &wc)` (SSID/pass copied from app_config into `wifi_sta_config_t.ssid/.password`; **`WIFI_IF_STA`, not the removed `ESP_IF_WIFI_STA`**) â†’ `esp_wifi_start()`.

Backoff state machine (no chip reset, ever):
- `WIFI_EVENT_STA_START` â†’ `esp_wifi_connect()`.
- `WIFI_EVENT_STA_DISCONNECTED` â†’ log `wifi_event_sta_disconnected_t.reason` **numerically** (do not switch on removed reason enums â€” `WIFI_REASON_ASSOC_EXPIRE`/`NOT_AUTHED`/`NOT_ASSOCED` are gone in v6); post `APP_EVENT_WIFI_LOST`; arm one-shot `esp_timer` with current `delay_ms`; then `delay_ms = MIN(delay_ms * 2, 60000)`.
- Timer callback: `esp_wifi_connect()` (nothing else â€” never blocks, never inits WiFi again; note v6: a second `esp_wifi_init()` returns `ESP_ERR_INVALID_STATE`, so there is exactly one init in `wifi_manager_start`, never in the retry path, and it is not wrapped in a retry loop).
- `IP_EVENT_STA_GOT_IP` â†’ cache IP string (`IPSTR`/`IP2STR` on `ip_info.ip`), reset `delay_ms = 1000`, post `APP_EVENT_WIFI_GOT_IP`, and start SNTP once (Â§2.9).

Sequence: 1 s â†’ 2 s â†’ 4 s â†’ 8 s â†’ 16 s â†’ 32 s â†’ 60 s â†’ 60 s â†’ â€¦ Handlers never delay in-line (they run on the event-loop task).

### 2.7 `sensor_driver.h` + `sht31` â€” swappable driver interface

```c
// sensor_driver.h â€” the seam for a future DHT22
typedef struct sensor_driver {
    const char *model;                                    // "SHT31"
    const char *kind;                                     // "temperature_humidity"
    esp_err_t (*init)(struct sensor_driver *drv);         // bus/dev setup; may allocate ctx
    esp_err_t (*probe)(struct sensor_driver *drv);        // ESP_OK | ESP_ERR_NOT_FOUND
    esp_err_t (*read)(struct sensor_driver *drv, float *temp_c, float *rh);
    void *ctx;
} sensor_driver_t;

sensor_driver_t *sht31_get_driver(uint8_t sda_gpio, uint8_t scl_gpio);  // sht31.h
```

`sht31.c` implementation (all APIs verified):
- `init`: `i2c_new_master_bus(&(i2c_master_bus_config_t){ .i2c_port = -1, .sda_io_num = (gpio_num_t)sda, .scl_io_num = (gpio_num_t)scl /* cast: fields are gpio_num_t */, .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true }, &bus)`; then `i2c_master_bus_add_device(bus, &(i2c_device_config_t){ .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x44, .scl_speed_hz = 100000 }, &dev)`.
- `probe`: `i2c_master_probe(bus, 0x44, 100)`; on failure try `0x45` (re-add device on the alt address if found). Distinguish logs: `ESP_ERR_NOT_FOUND` = "no SHT31 on bus", `ESP_ERR_TIMEOUT` = "bus stuck / missing pull-ups".
- `read` (single-shot, high repeatability): `i2c_master_transmit(dev, (uint8_t[]){0x24, 0x00}, 2, 100)` â†’ `vTaskDelay(pdMS_TO_TICKS(16))` (16 ms exact at 1000 Hz tick â€” we set `CONFIG_FREERTOS_HZ=1000`) â†’ `i2c_master_receive(dev, buf, 6, 100)`. **Not** `i2c_master_transmit_receive` (repeated-START, no room for the 15 ms conversion â€” verified caveat). CRC-8 (poly 0x31, init 0xFF) over each 2-byte word; mismatch â‡’ `ESP_ERR_INVALID_CRC`. Convert: `T = -45 + 175*raw/65535`, `RH = 100*raw/65535`.
- **v6 pitfall handled here:** NACK returns `ESP_ERR_INVALID_RESPONSE` (not `ESP_ERR_INVALID_STATE`) â€” treat it as "sensor vanished", count 3 consecutive failures â‡’ log error, keep trying at the normal period (self-heals if re-attached; discovery is not retro-changed in Phase 1).
- Explicit `#include "freertos/FreeRTOS.h"` + `"freertos/task.h"` in `sht31.c` (driver headers no longer provide them).

### 2.8 `sensor_task` â€” probe-at-boot, absent-tolerant

```c
esp_err_t sensor_task_start(void);   // returns ESP_OK even when sensor absent
bool      sensor_present(void);      // consumed by discovery
```

`sensor_task_start()` (main-task context, before WiFi): get driver via `sht31_get_driver(cfg->i2c_sda, cfg->i2c_scl)`, `init()`, `probe()`. 
- **Absent** (user's current hardware): `ESP_LOGW(TAG, "SHT31 not found (%s) - sensor disabled, discovery will omit sensor capability", esp_err_to_name(err))`; tear down the I2C device/bus handles; set `present=false`; **do not create the task**. Node continues normally.
- Present: `xTaskCreate(sensor_task_fn, "sensor", 4096, NULL, 4, NULL)` (stack in **bytes**).

Task loop: `esp_task_wdt_add(NULL)`; then forever: sample; validate range (âˆ’20â€¦80 Â°C, 0â€¦100 %RH; out-of-range â‡’ `ESP_LOGW(TAG, "discarding out-of-range sample T=%.1f RH=%.1f", t, h)` and skip publish); build payload with `snprintf(buf, sizeof buf, "{\"temperature\":%.1f,\"humidity\":%.1f,\"ts\":%lld}", t, h, (long long)time(NULL))` (no cJSON needed here); `mqtt_mgr_publish("sensor/state", buf, 0, false)` (QoS 0, not retained, silently skipped while offline). Then wait `sensor_interval_s` in **1 s `vTaskDelay` slices with `esp_task_wdt_reset()` each slice** â€” keeps the 30 s period compatible with the 10 s TWDT. (Periodic drift is irrelevant here; if exact periods are wanted later, `xTaskDelayUntil` â€” `vTaskDelayUntil` is **removed** in v6.)

### 2.9 Time (`ts` field)

After first `IP_EVENT_STA_GOT_IP`, wifi_manager starts SNTP once: `esp_netif_sntp_init(&(esp_netif_sntp_config_t)ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org"))` â€” header `esp_netif_sntp.h` (v6 note: legacy `sntp.h` was removed; `esp_sntp.h`/`esp_netif_sntp.h` are the survivors). **Verify at first build** (the reports didn't cover this header's exact location in v6). Fallback if it fights the build: publish `ts` from `time(NULL)` regardless (pre-sync values are small epoch numbers; server side already timestamps on receipt) and mark SNTP a fast-follow. This is deliberately isolated to one call in wifi_manager so the fallback is a one-line change.

### 2.10 `discovery` â€” cJSON payload

```c
esp_err_t discovery_publish(void);   // called only from MQTT_EVENT_CONNECTED
```

Build with cJSON (API unchanged; managed `espressif/cjson`, include `"cJSON.h"`): root object with `node_id`, `room`, `fw_version` (= `esp_app_get_description()->version`, header `esp_app_desc.h`, component `esp_app_format` â€” the v6 replacement for removed `esp_ota_get_app_description`), `ip` (= `wifi_manager_get_ip_str()`), `capabilities` array: always the two relay entries `{ "type":"relay", "channel":1|2, "name":cfg->relay_name[i] }`; append the sensor entry `{ "type":"sensor","channel":1,"kind":"temperature_humidity","model":"SHT31","interval_s":cfg->sensor_interval_s }` **only if `sensor_present()`** â€” the spec's sensor-absent requirement. Serialize `cJSON_PrintUnformatted`, publish via `mqtt_mgr_publish("discovery", json, 1, true)`, then `cJSON_free(printed)` + `cJSON_Delete(root)`. Discovery is re-published on every reconnect (retained anyway, but this refreshes `ip` after DHCP changes).

## 3. `sdkconfig.defaults` â€” exact proposed content

```
# --- Target / flash / partitions -------------------------------------------
CONFIG_IDF_TARGET="esp32s3"
# esptool default is 2MB; S3 modules are >=4MB. 4MB is safe for every common
# S3 module (N4/N8/N16). If `idf.py flash` reports 8MB/16MB, bump this.
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

# --- RTOS timing ------------------------------------------------------------
# Default 100Hz => 10ms ticks: too coarse for the 16ms SHT31 conversion wait
# and queue timeouts. 1000Hz makes pdMS_TO_TICKS exact at 1ms.
CONFIG_FREERTOS_HZ=1000

# --- Task watchdog (req. 8) -------------------------------------------------
# WDT_EN/WDT_INIT default y (kept implicit). Default PANIC=n only logs;
# a field device must reboot on a hung task.
CONFIG_ESP_TASK_WDT_PANIC=y
# 5s default is tight around blocking NVS commits + MQTT enqueue; 10s is safe.
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10

# --- Logging (req. 7) -------------------------------------------------------
CONFIG_LOG_DEFAULT_LEVEL_INFO=y

# --- Stacks -----------------------------------------------------------------
# app_main runs the whole init chain incl. NVS seeding and I2C probe.
CONFIG_ESP_MAIN_TASK_STACK_SIZE=4096
# Default 2304 is marginal: our WIFI/IP/APP_EVENT handlers run here and call
# into mqtt start, cJSON-free paths and NVS.
CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096

# --- Bring-up escape hatches (v6: default warnings are ERRORS, GCC 15) ------
# Uncomment ONLY if the first build trips on toolchain-new warnings, then fix
# the code and re-comment before Phase-1 sign-off:
# CONFIG_COMPILER_DISABLE_GCC15_WARNINGS=y
# CONFIG_COMPILER_DISABLE_DEFAULT_ERRORS=y
```

`partitions.csv` (fits 4 MB: `0x10000` app offset + 3 MB factory = `0x310000` < `0x400000`; nvs 24 KB matches the stock singleapp size and is ample for our ~25 small keys):
```
# Name,     Type, SubType, Offset,  Size,   Flags
nvs,        data, nvs,     ,        0x6000,
phy_init,   data, phy,     ,        0x1000,
factory,    app,  factory, ,        3M,
```
No OTA slots in Phase 1 (roadmap doesn't require OTA yet); the custom CSV exists so adding `ota_0/ota_1` later is a CSV edit, not a layout migration.

Deliberately **not** set: `CONFIG_APP_PROJECT_VER*` (version comes from CMake `PROJECT_VER`, Â§4 â€” setting both is a precedence trap: the Kconfig pair silently wins); `CONFIG_LIBC_NEWLIB_NANO_FORMAT` (touch only if `%.1f` prints empty â€” see step 2 smoke test).

## 4. Build files

**Root `firmware/CMakeLists.txt`** (v6-correct: CMake â‰¥3.22; `PROJECT_VER` set *before* including `project.cmake` is the documented version mechanism â€” `project(... VERSION x.y.z)` is not; `MINIMAL_BUILD` left at default OFF during bring-up so `main` auto-requires all components and a missing REQUIRES can't break linking â€” enable later as an optimization):
```cmake
cmake_minimum_required(VERSION 3.22)
set(PROJECT_VER "1.0.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(smart_home_node)
```

**`main/idf_component.yml`** (both resolve OFFLINE from `file://C:\Espressif\tools` mirror: mqtt `1.0.0`, cjson `1.7.19~2`; no `esp_wifi_remote` â€” its rule only fires on esp32p4/esp32h2):
```yaml
dependencies:
  idf:
    version: ">=6.0"
  espressif/mqtt: "^1.0.0"
  espressif/cjson: "^1.7.19"
```

**`main/CMakeLists.txt`** (component names per the v6 split; `mqtt` listed explicitly per the mesh-example pattern for clarity; **no** `json`/`cjson` (injected), **no** `driver` (legacy husk, no re-exports), **no** `freertos`/`log`/`esp_system` (common components)):
```cmake
idf_component_register(
    SRCS "main.c" "app_events.c" "app_config.c" "wifi_manager.c" "mqtt_mgr.c"
         "relay_driver.c" "button_handler.c" "sensor_task.c" "sht31.c" "discovery.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_timer esp_event
                  esp_wifi esp_netif nvs_flash esp_hw_support esp_app_format mqtt)
```
(`esp_hw_support` for `esp_mac.h`; `esp_app_format` for `esp_app_desc.h`; `esp_task_wdt.h` needs nothing â€” `esp_system` is common. If SNTP survives (Â§2.9), `lwip` may need adding to PRIV_REQUIRES â€” check at first build.)

**`.gitignore`**: `build/`, `sdkconfig`, `sdkconfig.old`, `managed_components/`. **Commit** `dependencies.lock` (pins the exact mirror versions â‡’ reproducible offline builds).

## 5. v6 pitfalls checklist (executor-facing, condensed)

1. **Explicit FreeRTOS includes** in every file using tasks/queues/delays â€” driver headers and `esp_event.h` no longer include them (`freertos/FreeRTOS.h` first, then `task.h`/`queue.h`/`semphr.h`).
2. **`esp_driver_gpio` / `esp_driver_i2c`** in REQUIRES â€” never `driver`.
3. **mqtt + cjson are managed components** â€” manifest, not in-tree; `cjson` never appears in REQUIRES; scripted builds need `IDF_COMPONENT_LOCAL_STORAGE_URL=file://C:\Espressif\tools` in the environment for offline resolution.
4. **Warnings are errors** under GCC 15 â€” expect `-Wformat`/unused hits on first build; fix, or temporarily enable the two commented escape hatches.
5. **I2C NACK â‡’ `ESP_ERR_INVALID_RESPONSE`** (not `ESP_ERR_INVALID_STATE`); `i2c_master_probe` â‡’ `ESP_ERR_NOT_FOUND`/`ESP_ERR_TIMEOUT`.
6. **WiFi**: use `WIFI_IF_STA`; removed reason codes (`WIFI_REASON_ASSOC_EXPIRE`, `WIFI_REASON_NOT_AUTHED`, `WIFI_REASON_NOT_ASSOCED`) must not appear; second `esp_wifi_init()` returns `ESP_ERR_INVALID_STATE`; always `WIFI_INIT_CONFIG_DEFAULT()`.
7. **`vTaskDelayUntil` removed** â†’ `xTaskDelayUntil`; stack sizes in **bytes**.
8. **`esp_ota_get_app_description` removed** â†’ `esp_app_get_description()` from `esp_app_desc.h`.
9. **`sntp.h` removed** â†’ `esp_sntp.h`/`esp_netif_sntp.h`.
10. **`esp_mqtt_client_subscribe` is a `_Generic` macro** â†’ call `esp_mqtt_client_subscribe_single` directly.
11. `event->topic` is **not NUL-terminated** and only valid on the first `MQTT_EVENT_DATA` fragment â€” length-bounded compare + `total_data_len == data_len` guard.
12. Never call `esp_task_wdt_init()` (auto-inited); use `esp_task_wdt_add(NULL)`/`esp_task_wdt_reset()`.

## 6. Ordered implementation steps with smoke tests

Environment per step â€” interactive: `. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1` then `idf.py` from `firmware\`. Scripted: `$env:IDF_COMPONENT_LOCAL_STORAGE_URL="file://C:\Espressif\tools"; & "C:\Espressif\tools\python\v6.0.2\venv\Scripts\python.exe" "C:\Espressif\esp\v6.0.2\esp-idf\tools\idf.py" -C C:\Users\Izuki\Projects\SmartHomeController\firmware <args>`. Board on **COM4**. Broker = dockerized `eclipse-mosquitto:2` on the LAN host with user/pass auth (from the repository-root `compose.yml`); all `mosquitto_sub/pub` below implicitly carry `-h <BROKER_IP> -p 1883 -u <user> -P <pass>`.

1. **Scaffold + first build (no hardware).** Create all files in Â§1 with empty-but-compiling modules. `idf.py set-target esp32s3` (regenerates sdkconfig from defaults) â†’ `idf.py build`. Verify: build succeeds; `managed_components/espressif__mqtt` and `espressif__cjson` appear (offline); sanity-diff `managed_components/espressif__mqtt/include/mqtt_client.h` field names against Â§2.5 (they will match â€” same zip); binary size fits `factory` partition (`idf.py size`).
2. **app_config + main.c.** Implement NVS seeding/load, node_id. `idf.py -p COM4 flash monitor`: first boot logs "seeding config from Kconfig defaults", prints full config + node_id; second boot logs "config loaded (ver 1)". Also `printf("%.1f", 25.5f)` a test value once to confirm float formatting (pitfall Â§0.3), then remove.
3. **relay_driver + button_handler (offline core).** Verify in monitor: boot applies power-on behavior; button press toggles relay within ~50â€“100 ms (audible click / LED / multimeter on GPIO 4/5); rapid presses debounce cleanly; `idf.py -p COM4 monitor` after reset with `restore` shows the previous state re-applied. WiFi not even configured yet â€” proves local control has zero network dependency.
4. **wifi_manager.** Set real SSID via `idf.py menuconfig` â†’ erase-flash â†’ flash (re-seeds NVS). Verify: got-IP log <10 s; then test backoff: power off the AP â€” logs show reconnect attempts at 1/2/4/â€¦/60 s spacing, no reboot; power AP back on â€” reconnects, delay resets.
5. **mqtt_mgr + discovery (connect sequence).** Run `mosquitto_sub -t 'home/#' -v` first, then reset the board. Expected within ~10 s, in order: `.../status online`, `.../discovery {...}` (with **no** sensor capability â€” sensor absent), `.../relay/1/state`, `.../relay/2/state`. Verify retained: restart `mosquitto_sub` â€” all four replay instantly.
6. **Relay over MQTT + source attribution.** `mosquitto_pub -q 1 -t 'home/livingroom/esp32s3-XXXXXX/relay/1/set' -m '{"state":"ON"}'` â†’ relay clicks, `relay/1/state` shows `{"state":"ON","source":"mqtt"}`. Press button 1 â†’ state with `"source":"button"`. Malformed payload (`-m 'garbage'`) â†’ ESP_LOGW, no crash, no state change.
7. **cmd + LWT.** `mosquitto_pub -q 1 -t '.../cmd' -m '{"action":"reboot"}'` â†’ board publishes `offline`, reboots, full connect sequence replays. LWT: yank board power while subscribed to `.../status` â†’ `offline` (retained) appears within ~45 s (keepalive 30 Ã— 1.5).
8. **sensor_task.** With no sensor attached (user's current state): boot log shows the SHT31-absent warning, no `sensor` capability in discovery, no `sensor/state` traffic, node fully functional â€” this **is** the Phase-1 acceptance configuration. (When an SHT31 arrives: wire to GPIO 8/9 + 3V3/GND, reboot, verify capability appears and `sensor/state` publishes every 30 s; unplug mid-run â†’ 3-failure error log, no crash.)
9. **Hardening pass.** Confirm TWDT: temporarily insert an infinite loop in relay_task â†’ panic+reboot within 10 s, then remove. Remove any bring-up escape hatches from sdkconfig.defaults, rebuild clean, run the full Â§7 acceptance list.

## 7. Verification â€” Phase-1 acceptance criteria mapping

| Criterion | Exact procedure | Needs board? |
|---|---|---|
| **Discovery <10 s after boot** | `mosquitto_sub -h <IP> -u <u> -P <p> -t 'home/+/+/discovery' -v` running; press board RST; stopwatch from release to JSON line. Pass: <10 s (expect 4â€“7 s: WiFi ~3 s + DHCP + MQTT ~1 s). Validate JSON fields against the contract (node_id regex `esp32s3-[0-9a-f]{6}`, fw_version `1.0.0`, ip, 2 relay capabilities, **no sensor entry**). | Yes |
| **Relay toggle <1 s end-to-end** | `mosquitto_sub -t 'home/+/+/relay/1/state' -v` in one shell; in another: `mosquitto_pub -q 1 -t 'home/<room>/<node>/relay/1/set' -m '{"state":"OFF"}'`; audible click and state message follow effectively instantly (<200 ms typical). | Yes |
| **Button works offline + resyncs** | `docker stop <mosquitto-container>` (or stop AP). Press button â†’ relay clicks immediately. `docker start <mosquitto-container>`; within reconnect window (â‰¤60 s WiFi backoff cap + 5 s MQTT retry) the connect sequence republishes `relay/{ch}/state` reflecting the offline toggle â€” verify with retained read: `mosquitto_sub -t 'home/#' -v -C 6`. | Yes |
| **LWT offline â‰¤90 s** | Subscribe `.../status`; cut board power (not RST â€” RST also works but tests the same path). `offline` retained arrives â‰¤45 s (keepalive 30 s Ã— 1.5), inside the 90 s budget. | Yes |
| **Relay state restore after power cycle** | With `poweron=restore` (default): set relay 1 ON via MQTT, cut power 10 s, repower. Relay 1 physically ON immediately at boot (before WiFi), and after reconnect `relay/1/state` retained shows ON. | Yes |
| **No-hardware fallback** | `idf.py build` green with zero warnings-escape-hatches enabled + review of `idf.py size` fit is the full CI-equivalent check. Everything above except this row needs the physical board on COM4 + the dockerized broker; QEMU is explicitly not required. | No |

### Critical Files for Implementation
- C:\Users\Izuki\Projects\SmartHomeController\firmware\main\mqtt_mgr.c
- C:\Users\Izuki\Projects\SmartHomeController\firmware\main\relay_driver.c
- C:\Users\Izuki\Projects\SmartHomeController\firmware\main\app_config.c
- C:\Users\Izuki\Projects\SmartHomeController\firmware\main\CMakeLists.txt
- C:\Users\Izuki\Projects\SmartHomeController\firmware\sdkconfig.defaults
