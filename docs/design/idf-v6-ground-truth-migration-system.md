# ESP-IDF v6.0.2 Ground Truth Report â€” ESP32-S3 Smart-Home Node

Source of truth: `C:\Espressif\esp\v6.0.2\esp-idf` (read directly; nothing below is from memory of v5.x).

---

## 0. Version confirmation

`C:\Espressif\esp\v6.0.2\esp-idf\components\esp_common\include\esp_idf_version.h`

```c
#define ESP_IDF_VERSION_MAJOR   6
#define ESP_IDF_VERSION_MINOR   0
#define ESP_IDF_VERSION_PATCH   2
```

Confirmed **v6.0.2**. Migration guides live at `docs\en\migration-guides\release-6.x\6.0\` (note the extra `6.0` subdirectory).

---

## 1. ðŸ”´ SHOWSTOPPERS vs. a v5.x-written spec

These three break the spec outright. Address them before anything else.

### 1.1 esp-mqtt is NO LONGER part of ESP-IDF

`docs\...\6.0\protocols.rst` (ESP-MQTT section):

> "The ESP-MQTT component has been removed from ESP-IDF and is now a managed component: `espressif/mqtt`."

Verified on disk:
- `C:\Espressif\esp\v6.0.2\esp-idf\components\mqtt\` contains **only** `test_apps/` (two `sdkconfig.ci.default` files). No `CMakeLists.txt`, no sources, no headers.
- `find ... -name mqtt_client.h` over the entire IDF tree returns **nothing**.

Consequences:
- `mqtt` must **NOT** appear in `REQUIRES`/`PRIV_REQUIRES`. It is not a component in this tree.
- You must add it via the component manager: `idf.py add-dependency "espressif/mqtt^1.0.0"`, or hand-write `main/idf_component.yml`.
- **This requires network access on first build.** There is no cached copy under `C:\Users\Izuki\.espressif\` (checked â€” only `dist`, `espidf.constraints.v6.0.txt`, `idf-env.json`, `python_env`, `tools`). Flag this as a build prerequisite.
- `mqtt_client.h` include path and API are unchanged per the migration guide, but **the header is not present locally, so I cannot verify `esp_mqtt_client_config_t` field names (LWT, credentials, session) against this tree.** Treat any LWT/auth struct-field details as unverified until the component is fetched.

Canonical manifest from `examples\protocols\mqtt\main\idf_component.yml`:

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

And `examples\protocols\mqtt\main\CMakeLists.txt` â€” note `mqtt` is absent from `PRIV_REQUIRES`:

```cmake
idf_component_register(
    SRCS "app_main.c"
    PRIV_REQUIRES esp-tls esp_wifi esp_event nvs_flash
    INCLUDE_DIRS "."
    EMBED_TXTFILES "mosquitto.org.crt"
)
```

Also: the old `examples/protocols/mqtt/tcp`, `/ssl`, etc. subdirectories are gone. There is now a single flattened `examples/protocols/mqtt` (TLS) and `examples/protocols/mqtt5`.

### 1.2 The built-in `json` (cJSON) component is REMOVED

`protocols.rst`:
> "The built-in `json` component has been removed from ESP-IDF."

Verified: no `components/json`, no `cJSON.h` anywhere in the tree.

For the JSON discovery payload you must either:
- add `espressif/cjson: "^1.7.19"` to `main/idf_component.yml` (another network dependency), **or**
- hand-roll the discovery payload with `snprintf` (viable â€” a Home-Assistant-style discovery blob is a fixed template; this avoids a second managed dependency).

Remove `json` from any `REQUIRES`/`PRIV_REQUIRES` inherited from the v5.x spec. The API is unchanged if you do pull in `espressif/cjson`.

### 1.3 Default warnings are now errors

`build-system.rst`:
> "The default compiler warnings will be considered as errors. The configuration option `CONFIG_COMPILER_DISABLE_DEFAULT_ERRORS` has been changed to N."

Plus GCC upgraded 14.2.0 â†’ **15.1.0** (`toolchain.rst`), adding new warnings. Escape hatch: `CONFIG_COMPILER_DISABLE_GCC15_WARNINGS`. Budget for warning cleanup, or set `CONFIG_COMPILER_DISABLE_DEFAULT_ERRORS=y` in `sdkconfig.defaults` during bring-up.

Also new: **linker orphan sections are now a hard error**. Escape hatch `CONFIG_COMPILER_ORPHAN_SECTIONS` = `warning`/`place`.

---

## 2. Driver component split â€” GPIO and I2C

### 2.1 What goes in `idf_component_register`

| Feature | Component name for `REQUIRES`/`PRIV_REQUIRES` | Public header |
|---|---|---|
| GPIO (relays, buttons, ISR) | `esp_driver_gpio` | `driver/gpio.h` |
| I2C master (SHT31) | `esp_driver_i2c` | `driver/i2c_master.h` |
| Legacy I2C (do not use) | `driver` | `driver/i2c.h` |
| Task watchdog | *(none â€” `esp_system` is a common component)* | `esp_task_wdt.h` |
| esp_timer (debounce) | `esp_timer` | `esp_timer.h` |
| esp_event | `esp_event` | `esp_event.h` |
| NVS | `nvs_flash` | `nvs_flash.h`, `nvs.h` |
| WiFi | `esp_wifi` | `esp_wifi.h` |
| netif | `esp_netif` (public dep of `esp_wifi`) | `esp_netif.h` |
| FreeRTOS | *(common â€” never list it)* | `freertos/FreeRTOS.h` etc. |

Recommended `main/CMakeLists.txt` dependency line:

```cmake
PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_timer esp_event esp_wifi nvs_flash
```

`esp-tls` only if you do MQTT over TLS.

### 2.2 Common components (auto-required, never list them)

`tools\cmake\build.cmake` lines 329â€“330 (authoritative, more complete than the doc):

```cmake
set(requires_common cxx esp_libc freertos esp_hw_support heap log soc hal
             esp_rom esp_common esp_system esp_stdio)
```

plus `xtensa` (the arch, appended at line 645). Note `esp_libc` â€” the `newlib` component was **renamed**. `esp_stdio` is the former `esp_vfs_console`. `esp_system` being common is why `esp_task_wdt.h` needs no explicit REQUIRES.

### 2.3 Does `driver/i2c.h` still exist? â€” YES, but EOL

**It exists**: `components\driver\i2c\include\driver\i2c.h` and `i2c_types_legacy.h`, with `i2c/i2c.c` still compiled.

`peripherals.rst`:
> "The legacy I2C driver (`driver/i2c.h`) has been marked as **End-of-Life (EOL)** in ESP-IDF v6.0 and is scheduled for **removal in v7.0**." â€” "ESP-IDF will not provide updates, bug fixes, or security patches for the legacy driver timely."

**Use `driver/i2c_master.h` for SHT31.** Suppression option if you ever need the old one: `CONFIG_I2C_SUPPRESS_DEPRECATE_WARN` (also `CONFIG_I2C_SKIP_LEGACY_CONFLICT_CHECK`), in menu `Component config > Legacy Driver Configurations > Legacy I2C Driver Configurations`.

The `driver` component in v6 is now a husk: only `i2c/`, `touch_sensor/`, `twai/`. Everything else (SPI, UART, LEDC, RMT, GPTimer, PCNT, I2S, DAC, ADC, tsens, sdmâ€¦) has been fully removed from it and lives in `esp_driver_*`. It also **no longer publicly re-exports** those components, so a stale `REQUIRES driver` will not silently pull in what it used to.

### 2.4 ðŸŸ¡ New in v6: FreeRTOS headers no longer implicitly included

`peripherals.rst`:
> "Starting from v6.0, to improve the portability of IDF drivers, all public driver header files no longer include operating-systemâ€“specific (FreeRTOS) headers."

And `system.rst`, ESP-Event section:
> "Unnecessary FreeRTOS headers have been removed from `esp_event.h`. Code that previously depended on these implicit includes must now include the headers explicitly: `#include "freertos/queue.h"` and `#include "freertos/semphr.h"`."

**Action for our code:** every module using queues/semaphores/tasks must explicitly include:

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
```

This will silently bite v5.x-derived source files. (`esp_task_wdt.h` is an exception â€” it still includes `freertos/FreeRTOS.h` and `freertos/task.h` itself.)

### 2.5 New HAL component split (informational)

v6 also introduces `esp_hal_gpio`, `esp_hal_i2c`, `esp_hal_timg`, `esp_hal_wdt`, â€¦ as separate components. You do **not** need to list them: `esp_driver_gpio` has `REQUIRES esp_hal_gpio` and `esp_driver_i2c` has `REQUIRES esp_hal_i2c`, both public.

---

## 3. GPIO API (verbatim from `components\esp_driver_gpio\include\driver\gpio.h`)

```c
typedef void (*gpio_isr_t)(void *arg);

typedef struct {
    uint64_t pin_bit_mask;          /*!< GPIO pin: set with bit mask, each bit maps to a GPIO */
    gpio_mode_t mode;               /*!< GPIO mode: set input/output mode                     */
    gpio_pullup_t pull_up_en;       /*!< GPIO pull-up                                         */
    gpio_pulldown_t pull_down_en;   /*!< GPIO pull-down                                       */
    gpio_int_type_t intr_type;      /*!< GPIO interrupt type                                  */
    gpio_hys_ctrl_mode_t hys_ctrl_mode;  /*!< (conditional) GPIO hysteresis                   */
} gpio_config_t;
```

Field names are **unchanged** from v5.x. Functions we need, all present and unchanged:

```c
esp_err_t gpio_config(const gpio_config_t *pGPIOConfig);
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level);
esp_err_t gpio_install_isr_service(int intr_alloc_flags);
esp_err_t gpio_isr_handler_add(gpio_num_t gpio_num, gpio_isr_t isr_handler, void *args);
esp_err_t gpio_isr_handler_remove(gpio_num_t gpio_num);
esp_err_t gpio_set_intr_type(gpio_num_t gpio_num, gpio_int_type_t intr_type);
esp_err_t gpio_intr_enable(gpio_num_t gpio_num);
esp_err_t gpio_intr_disable(gpio_num_t gpio_num);
esp_err_t gpio_reset_pin(gpio_num_t gpio_num);
```

v6 GPIO changes (from `peripherals.rst` + header):
- `gpio_uninstall_isr_service()` now returns `esp_err_t` (was `void`). Harmless unless you assigned its result.
- `gpio_iomux_in`/`gpio_iomux_out` â†’ private (`esp_private/gpio.h`), renamed `gpio_iomux_input`/`gpio_iomux_output`.
- `MAX_PAD_GPIO_NUM`, `MAX_GPIO_NUM`, `DIG_IO_HOLD_BIT_SHIFT` removed.
- Deep-sleep wakeup APIs removed: `gpio_deep_sleep_wakeup_enable` â†’ `gpio_wakeup_enable_on_hp_periph_powerdown_sleep`; macro `GPIO_IS_DEEP_SLEEP_WAKEUP_VALID_GPIO()` â†’ `GPIO_IS_HP_PERIPH_PD_WAKEUP_VALID_IO()`. (Not needed unless we add sleep.)
- New helpers available: `gpio_input_enable`, `gpio_output_enable`, `gpio_output_disable`, `gpio_od_enable`, `gpio_get_io_config()`, `gpio_dump_io_configuration()` â€” the last is handy for debugging relay/button pin setup.

---

## 4. I2C master API for SHT31 (`components\esp_driver_i2c\include\driver\i2c_master.h`)

### 4.1 Config structs â€” verbatim field names

```c
typedef struct {
    i2c_port_num_t i2c_port;              /*!< I2C port number, `-1` for auto selecting */
    gpio_num_t sda_io_num;                /*!< GPIO number of I2C SDA signal */
    gpio_num_t scl_io_num;                /*!< GPIO number of I2C SCL signal */
    union {
        i2c_clock_source_t clk_source;    /*!< Clock source of I2C master bus */
        lp_i2c_clock_source_t lp_source_clk;  /* only if SOC_LP_I2C_SUPPORTED */
    };
    uint8_t glitch_ignore_cnt;            /*!< typically 7 */
    int intr_priority;                    /*!< 0 = driver picks (1,2,3) */
    size_t trans_queue_depth;             /*!< async only */
    struct {
        uint32_t enable_internal_pullup: 1;
        uint32_t allow_pd:               1;
    } flags;
} i2c_master_bus_config_t;

typedef struct {
    i2c_addr_bit_len_t dev_addr_length;   /*!< I2C_ADDR_BIT_LEN_7 */
    uint16_t device_address;              /*!< 7/10-bit addr WITHOUT R/W bit */
    uint32_t scl_speed_hz;
    uint32_t scl_wait_us;                 /*!< 0 = use default reg value */
    struct {
        uint32_t disable_ack_check:      1;
    } flags;
} i2c_device_config_t;
```

âš ï¸ **`sda_io_num`/`scl_io_num` are `gpio_num_t`, not `int`.** Use `GPIO_NUM_8`, not `8`. (Matches the v6 type-safety push seen in LCD too.)

### 4.2 Functions

```c
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *bus_config, i2c_master_bus_handle_t *ret_bus_handle);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus_handle, const i2c_device_config_t *dev_config, i2c_master_dev_handle_t *ret_handle);
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t i2c_dev, const uint8_t *write_buffer, size_t write_size, int xfer_timeout_ms);
esp_err_t i2c_master_receive(i2c_master_dev_handle_t i2c_dev, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t i2c_dev, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int xfer_timeout_ms);
esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus_handle, uint16_t address, int xfer_timeout_ms);
esp_err_t i2c_master_bus_reset(i2c_master_bus_handle_t bus_handle);
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus_handle);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t handle);
```

### 4.3 ðŸŸ¡ v6 behavior change â€” NACK error code

`peripherals.rst`:
> Following functions now return **`ESP_ERR_INVALID_RESPONSE`** instead of `ESP_ERR_INVALID_STATE` when NACK from the bus is detected: `i2c_master_transmit`, `i2c_master_multi_buffer_transmit`, `i2c_master_transmit_receive`, `i2c_master_execute_defined_operations`.

**Our SHT31 driver's "sensor absent / bus fault" detection must check `ESP_ERR_INVALID_RESPONSE`.** A v5.x-era `if (err == ESP_ERR_INVALID_STATE)` check will misclassify a missing sensor.

Note SHT31 needs a command-then-delayed-read pattern (single-shot: write 2 cmd bytes, wait ~15 ms, then read 6 bytes). Use `i2c_master_transmit` + `vTaskDelay` + `i2c_master_receive` rather than `i2c_master_transmit_receive`, which issues a repeated START with no measurement delay.

### 4.4 ESP32-S3 I2C facts

From `components\soc\esp32s3\include\soc\soc_caps.h`: `SOC_I2C_NUM = 2`, `SOC_HP_I2C_NUM = 2`, `SOC_I2C_SUPPORT_10BIT_ADDR = 1`, `SOC_I2C_SUPPORT_SLAVE = 1`, **no LP I2C** on S3.

From `components\soc\esp32s3\include\soc\clk_tree_defs.h`:
```c
typedef enum {
    I2C_CLK_SRC_XTAL = SOC_MOD_CLK_XTAL,
    I2C_CLK_SRC_RC_FAST = SOC_MOD_CLK_RC_FAST,
    I2C_CLK_SRC_DEFAULT = SOC_MOD_CLK_XTAL,
} soc_periph_i2c_clk_src_t;
```

`I2C_ADDR_BIT_LEN_7 = 0` (`components\esp_hal_i2c\include\hal\i2c_types.h`). `i2c_port_num_t` is `typedef int`.

Relevant Kconfig (`components\esp_driver_i2c\Kconfig`, menu "ESP-Driver:I2C Configurations"): `CONFIG_I2C_ISR_IRAM_SAFE` (default n), `CONFIG_I2C_ENABLE_DEBUG_LOG` (default n), `CONFIG_I2C_MASTER_ISR_HANDLER_IN_IRAM` (**default y**).

### 4.5 No SHT31 driver ships with IDF

Confirmed: no `components/sht*` anywhere. Write the SHT3x driver in-house on `i2c_master.h` (recommended â€” it is ~150 lines including CRC-8), or pull an external managed component (another network dependency).

---

## 5. Task Watchdog (`components\esp_system\include\esp_task_wdt.h`)

Header location: **`components/esp_system/include/esp_task_wdt.h`** â€” no explicit REQUIRES needed (`esp_system` is common). Private variants exist at `esp_private/esp_task_wdt.h` and `esp_private/esp_task_wdt_impl.h`; do not use.

API and struct are **unchanged from v5.x**:

```c
typedef struct {
    uint32_t timeout_ms;        /**< TWDT timeout duration in milliseconds */
    uint32_t idle_core_mask;    /**< Bitmask of the core whose idle task should be subscribed */
    bool trigger_panic;         /**< Trigger panic when timeout occurs */
} esp_task_wdt_config_t;

typedef struct esp_task_wdt_user_handle_s * esp_task_wdt_user_handle_t;

esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t *config);
esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *config);
esp_err_t esp_task_wdt_deinit(void);
esp_err_t esp_task_wdt_add(TaskHandle_t task_handle);          /* NULL = current task */
esp_err_t esp_task_wdt_add_user(const char *user_name, esp_task_wdt_user_handle_t *user_handle_ret);
esp_err_t esp_task_wdt_reset(void);
esp_err_t esp_task_wdt_reset_user(esp_task_wdt_user_handle_t user_handle);
esp_err_t esp_task_wdt_delete(TaskHandle_t task_handle);
esp_err_t esp_task_wdt_delete_user(esp_task_wdt_user_handle_t user_handle);
esp_err_t esp_task_wdt_status(TaskHandle_t task_handle);       /* ESP_OK / ESP_ERR_NOT_FOUND / ESP_ERR_INVALID_STATE */
void __attribute__((weak)) esp_task_wdt_isr_user_handler(void);
esp_err_t esp_task_wdt_print_triggered_tasks(task_wdt_msg_handler msg_handler, void *opaque, int *cpus_fail);
```

âš ï¸ `esp_task_wdt_init()` "must only be called after the scheduler is started" and must not race between tasks.

### Kconfig (`components\esp_system\Kconfig`, lines 295â€“357) with **verified defaults**

| Option | Default | Notes |
|---|---|---|
| `CONFIG_ESP_TASK_WDT_EN` | **y** | Master enable |
| `CONFIG_ESP_TASK_WDT_INIT` | **y** | Auto-init at startup |
| `CONFIG_ESP_TASK_WDT_PANIC` | **n** | ðŸ‘‰ **Set `=y`** for a field device so a hung task reboots the node |
| `CONFIG_ESP_TASK_WDT_TIMEOUT_S` | **5** | range 1â€“60 |
| `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0` | **y** | |
| `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1` | **y** | depends on `!FREERTOS_UNICORE` |
| `CONFIG_ESP_TASK_WDT_USE_ESP_TIMER` | n on S3 | hidden; only default-y on ESP32-C2 |

Historical renames (from `components\esp_system\sdkconfig.rename`) â€” if the v5.x spec used the very old names: `CONFIG_TASK_WDT` â†’ `CONFIG_ESP_TASK_WDT_INIT`, `CONFIG_ESP_TASK_WDT` â†’ `CONFIG_ESP_TASK_WDT_INIT`, `CONFIG_TASK_WDT_PANIC` â†’ `CONFIG_ESP_TASK_WDT_PANIC`, `CONFIG_TASK_WDT_TIMEOUT_S` â†’ `CONFIG_ESP_TASK_WDT_TIMEOUT_S`.

Pattern for our design: since `ESP_TASK_WDT_INIT=y` auto-initializes with the idle tasks subscribed, our MQTT/sensor/button tasks just call `esp_task_wdt_add(NULL)` at task entry and `esp_task_wdt_reset()` each loop iteration. Do **not** call `esp_task_wdt_init()` again â€” it returns `ESP_ERR_INVALID_STATE`. Use `esp_task_wdt_reconfigure()` if you need a different timeout than 5 s.

âš ï¸ Interaction with a blocking MQTT/sensor loop: if a task blocks on `xQueueReceive` longer than the TWDT timeout it will trip. Either keep queue waits < timeout, or subscribe only tasks with bounded loop time.

---

## 6. esp_event and esp_timer â€” unchanged APIs

### esp_event (component `esp_event`)

`components\esp_event\CMakeLists.txt`: `REQUIRES log esp_common freertos`, `PRIV_REQUIRES esp_timer`.

Signatures present and unchanged:
```c
esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_event_handler_register(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg);
esp_err_t esp_event_handler_instance_register(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *instance);
esp_err_t esp_event_post(esp_event_base_t event_base, int32_t event_id, const void *event_data, size_t event_data_size, TickType_t ticks_to_wait);
```
`ESP_EVENT_DECLARE_BASE(id)` / `ESP_EVENT_DEFINE_BASE(id)` unchanged in `esp_event_base.h` â€” use these for our custom inter-module event bases.

**Only v6 change:** the implicit FreeRTOS includes are gone (see Â§2.4). Add `#include "freertos/queue.h"` / `"freertos/semphr.h"` where needed.

Kconfig: `CONFIG_ESP_EVENT_LOOP_PROFILING`, `CONFIG_ESP_EVENT_POST_FROM_ISR`, `CONFIG_ESP_EVENT_POST_FROM_IRAM_ISR` (renamed long ago from `CONFIG_EVENT_LOOP_PROFILING`, `CONFIG_POST_EVENTS_FROM_ISR`, `CONFIG_POST_EVENTS_FROM_IRAM_ISR`).

Default event loop task: `CONFIG_ESP_SYSTEM_EVENT_QUEUE_SIZE` default **32**, `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` default **2304**.

### esp_timer (component `esp_timer`) â€” this is our 50 ms debounce mechanism

```c
typedef struct esp_timer* esp_timer_handle_t;

typedef struct {
    esp_timer_cb_t callback;              //!< Callback function to execute when timer expires
    void* arg;                            //!< Argument to pass to callback
    esp_timer_dispatch_t dispatch_method; //!< Dispatch callback from task or ISR
    const char* name;                     //!< Timer name, used in esp_timer_dump()
    bool skip_unhandled_events;           //!< Skip unhandled events in light sleep for periodic timers
} esp_timer_create_args_t;

esp_err_t esp_timer_create(const esp_timer_create_args_t* create_args, esp_timer_handle_t* out_handle);
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period);
esp_err_t esp_timer_restart(esp_timer_handle_t timer, uint64_t timeout_us);
esp_err_t esp_timer_stop(esp_timer_handle_t timer);
esp_err_t esp_timer_delete(esp_timer_handle_t timer);
```

Unchanged from v5.x. `CONFIG_ESP_TIMER_TASK_STACK_SIZE` default **3584** (range 2048â€“65536).

**Debounce recommendation:** GPIO ISR (registered via `gpio_isr_handler_add`) calls `esp_timer_start_once(t, 50000)` â€” or `esp_timer_restart()` to re-arm on bounce â€” and returns immediately. The 50 ms callback samples `gpio_get_level()` and posts to the queue / event loop. This satisfies "no delays in ISR". `esp_timer_start_once`/`restart` are ISR-safe. `esp_timer` resolution is microseconds and independent of `FREERTOS_HZ`, which matters given the 100 Hz default tick (see Â§9).

---

## 7. Partition tables for ESP32-S3 (`components\partition_table\`)

Exact CSV contents (all `nvs` sizes verified verbatim):

**`partitions_singleapp.csv`** (default, `CONFIG_PARTITION_TABLE_SINGLE_APP`):
```
nvs,      data, nvs,     ,        0x6000,
phy_init, data, phy,     ,        0x1000,
factory,  app,  factory, ,        1M,
```

**`partitions_singleapp_large.csv`**:
```
nvs,      data, nvs,     ,        0x6000,
phy_init, data, phy,     ,        0x1000,
factory,  app,  factory, ,        1500K,
```

**`partitions_two_ota.csv`** â€” âš ï¸ note the **smaller** nvs:
```
nvs,      data, nvs,     ,        0x4000,
otadata,  data, ota,     ,        0x2000,
phy_init, data, phy,     ,        0x1000,
factory,  app,  factory, ,        1M,
ota_0,    app,  ota_0,   ,        1M,
ota_1,    app,  ota_1,   ,        1M,
```

**`partitions_two_ota_large.csv`** (no factory partition):
```
nvs,      data, nvs,     ,        0x6000,
otadata,  data, ota,     ,        0x2000,
phy_init, data, phy,     ,        0x1000,
ota_0,    app,  ota_0,   ,        1700K,
ota_1,    app,  ota_1,   ,        1700K,
```

**Answer to "what nvs size do they give":** `0x6000` (24 KB) for all singleapp variants and `two_ota_large`; `0x4000` (16 KB) for plain `two_ota`.

Key facts:
- There is **no flash-size-specific default CSV.** The same CSVs are used at 4 MB and 8 MB â€” nothing auto-expands to fill 8 MB. The stock tables total well under 2 MB.
- `CONFIG_PARTITION_TABLE_TYPE` default = `PARTITION_TABLE_SINGLE_APP`.
- `CONFIG_PARTITION_TABLE_OFFSET` default `0x8000`; `CONFIG_PARTITION_TABLE_MD5` default y.
- `CONFIG_ESPTOOLPY_FLASHSIZE` **defaults to `2MB`** â€” you *must* set `CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y` (or `_4MB`) in `sdkconfig.defaults` for an S3 module.
- Coredump-enabled builds auto-swap to `partitions_singleapp_coredump.csv` etc. when `ESP_COREDUMP_ENABLE_TO_FLASH` is on.

**Recommendation for this project:** a custom `partitions.csv` via `CONFIG_PARTITION_TABLE_CUSTOM=y` + `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"` (default filename value is `"partitions.csv"`, resolved relative to project root). Bump nvs to at least `0x6000`; with 8 MB you have ample room for two ~2 MB OTA slots plus a larger nvs.

---

## 8. Build system

### 8.1 Project CMakeLists skeleton (verified against `examples\get-started\hello_world\CMakeLists.txt`)

```cmake
# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(smart_home_node)
```

Two v6-era notes:
- **`cmake_minimum_required(VERSION 3.22)`** â€” `tools.rst`: "The minimal supported CMake version has been upgraded to 3.22.1." A v5.x spec likely says 3.16.
- **`idf_build_set_property(MINIMAL_BUILD ON)`** is now boilerplate in every IDF example. It restricts the build to common components + `main` + its transitive deps â†’ much faster builds. âš ï¸ Caveat from `build-system.rst`: with `MINIMAL_BUILD`, "ensure that all required components are specified in the `REQUIRES` or `PRIV_REQUIRES` argument during component registration" â€” a missing REQUIRES that used to work by accident will now fail to link. Also, `esp_psram` and `espcoredump` are not available by default under `MINIMAL_BUILD`.

`main/CMakeLists.txt`:
```cmake
idf_component_register(SRCS "app_main.c" "wifi_mgr.c" "mqtt_mgr.c" "relay.c" "button.c" "sht31.c" "cfg_nvs.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_timer esp_event esp_wifi nvs_flash esp-tls)
```

### 8.2 `idf.py set-target esp32s3`

`esp32s3` is in `SUPPORTED_TARGETS` (`tools\idf_py_actions\constants.py` line 45). Not a preview target (`PREVIEW_TARGETS = ['linux', 'esp32h21', 'esp32h4']`). Standard flow: `idf.py set-target esp32s3` (this wipes `sdkconfig` and regenerates from `sdkconfig.defaults`), then `idf.py build`.

### 8.3 `sdkconfig.defaults` mechanics (`docs\en\api-guides\build-system.rst` Â§1078â€“1129)

- `sdkconfig.defaults` in the project dir is applied "when creating a new config from scratch, or when any new config value hasn't yet been set in the `sdkconfig` file."
- Override the name / use multiple files via the `SDKCONFIG_DEFAULTS` env var or a `SDKCONFIG_DEFAULTS` variable in the top-level `CMakeLists.txt`. Non-absolute names resolve relative to the project dir.
- **If and only if** `sdkconfig.defaults` exists, the build also loads `sdkconfig.defaults.esp32s3`. If you have no generic defaults you must still create an empty `sdkconfig.defaults` for the target-specific file to be picked up.
- With multiple files in `SDKCONFIG_DEFAULTS`, each file's target-specific variant is applied immediately after it, before later files.
- `sdkconfig` itself should be gitignored; `sdkconfig.defaults` committed.

Suggested `sdkconfig.defaults` for this node:
```
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_ESP_TASK_WDT_PANIC=y
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
CONFIG_FREERTOS_HZ=1000
CONFIG_APP_PROJECT_VER_FROM_CONFIG=y
CONFIG_APP_PROJECT_VER="1.0.0"
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

### 8.4 Embedding a version string

Two mechanisms, both live (`docs\en\api-reference\system\misc_system_api.rst` Â§223â€“225, and `components\esp_app_format\Kconfig.projbuild` Â§26â€“36):

1. **CMake:** `set(PROJECT_VER "0.1.0.1")` in the project `CMakeLists.txt` **before** `include(.../project.cmake)`.
2. **Kconfig:** `CONFIG_APP_PROJECT_VER_FROM_CONFIG=y` + `CONFIG_APP_PROJECT_VER="1.0.0"` â€” this **takes precedence** over `PROJECT_VER`.

Fallback chain if neither is set: `$(PROJECT_PATH)/version.txt` â†’ `git describe` â†’ literal `"1"`.

âš ï¸ **`project(name VERSION x.y.z)` is not the documented mechanism** â€” the IDF path is the `PROJECT_VER` variable / Kconfig pair. Read it back at runtime with `esp_app_get_description()` from **`esp_app_desc.h`** (component `esp_app_format`).

ðŸŸ¡ v6 change: `esp_ota_get_app_description()` was **removed**; use `esp_app_get_description()`, include `esp_app_desc.h` (not `esp_ota_ops.h`), and add `esp_app_format` to your deps. Same for `esp_ota_get_app_elf_sha256` â†’ `esp_app_get_elf_sha256`. Useful for putting a firmware version into the MQTT discovery payload.

---

## 9. FreeRTOS configuration relevant to task creation

From `components\freertos\FreeRTOS-Kernel\include\freertos\task.h`:

> `@param usStackDepth The size of the task stack specified as the NUMBER OF **BYTES**. Note that this differs from vanilla FreeRTOS.`

**Confirmed: stack size argument to `xTaskCreate`/`xTaskCreatePinnedToCore` is in BYTES** in ESP-IDF (vanilla FreeRTOS uses words). Unchanged from v5.x, but worth stating explicitly since the spec may carry a word-based number.

Defaults verified in `components\freertos\Kconfig` and `components\esp_system\Kconfig`:

| Option | Default | Note |
|---|---|---|
| `CONFIG_FREERTOS_HZ` (`configTICK_RATE_HZ`) | **100** | range 1â€“1000. ðŸ‘‰ **Set to 1000** â€” at 100 Hz a tick is 10 ms, which is coarse for a 50 ms debounce window and for MQTT keepalive granularity |
| `CONFIG_FREERTOS_UNICORE` | **n** on ESP32-S3 | dual-core (only default-y on S2/linux) |
| `CONFIG_FREERTOS_IDLE_TASK_STACKSIZE` | 1536 bytes | |
| `CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH` | 2048 | |
| `CONFIG_FREERTOS_ISR_STACKSIZE` | 1536 (2096 if coredump) | per-core |
| `CONFIG_ESP_MAIN_TASK_STACK_SIZE` | 3584 bytes | stack of `app_main()` |
| `CONFIG_FREERTOS_CHECK_STACKOVERFLOW` | `..._CANARY` | |

### ðŸŸ¡ v6 FreeRTOS breaking changes (`system.rst`)

**Removed functions** â€” will not compile if the v5.x spec uses them:
- `xTaskGetAffinity` â†’ `xTaskGetCoreID`
- `xTaskGetIdleTaskHandleForCPU` â†’ `xTaskGetIdleTaskHandleForCore`
- `xTaskGetCurrentTaskHandleForCPU` â†’ `xTaskGetCurrentTaskHandleForCore`
- `xQueueGenericReceive` â†’ `xQueueReceive` / `xQueuePeek` / `xQueueSemaphoreTake`
- **`vTaskDelayUntil` â†’ `xTaskDelayUntil`** â† most likely to bite a periodic sensor-sampling loop
- `ulTaskNotifyTake`, `xTaskNotifyWait` â€” the compatibility *functions* removed; the *macros* of the same name remain, so source usually still compiles

**Deprecated:** `pxTaskGetStackStart` â†’ `xTaskGetStackStart`.

**Memory placement default flipped:** most FreeRTOS functions now default to **flash, not IRAM**. `CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH` was **removed**; the new opt-in is `CONFIG_FREERTOS_IN_IRAM` (default n). Same story for ringbuf: `CONFIG_RINGBUF_PLACE_FUNCTIONS_INTO_FLASH` removed â†’ `CONFIG_RINGBUF_IN_IRAM`. This saves IRAM at a small perf cost â€” fine for our node, but note it also now gates `CONFIG_SPI_MASTER_IN_IRAM`.

**Removed hidden options:** `CONFIG_FREERTOS_ENABLE_TASK_SNAPSHOT`, `CONFIG_FREERTOS_PLACE_SNAPSHOT_FUNS_INTO_FLASH`.

Task snapshot APIs moved: `freertos/task_snapshot.h` â†’ **`freertos/freertos_debug.h`**.

---

## 10. WiFi STA (component `esp_wifi`)

`components\esp_wifi\CMakeLists.txt`:
```cmake
REQUIRES esp_event esp_phy esp_netif
PRIV_REQUIRES esp_pm esp_timer nvs_flash efuse wpa_supplicant hal lwip esp_coex
```
So `PRIV_REQUIRES esp_wifi` transitively gives you `esp_event` and `esp_netif` headers. Reference: `examples\wifi\getting_started\station\main\CMakeLists.txt` uses exactly `PRIV_REQUIRES esp_wifi nvs_flash`.

### ðŸŸ¡ v6 WiFi breaking changes affecting reconnect-backoff logic (`wifi.rst`)

**Disconnect reason codes removed** â€” these are exactly what a backoff state machine switches on:
- `WIFI_REASON_ASSOC_EXPIRE` â†’ use **`WIFI_REASON_AUTH_EXPIRE`**
- `WIFI_REASON_NOT_AUTHED` â†’ use **`WIFI_REASON_CLASS2_FRAME_FROM_NONAUTH_STA`**
- `WIFI_REASON_NOT_ASSOCED` â†’ use **`WIFI_REASON_CLASS3_FRAME_FROM_NONASSOC_STA`**

Any v5.x reconnect handler naming the first set will fail to compile. Audit the `WIFI_EVENT_STA_DISCONNECTED` handler.

**Other removals:**
- Header `esp_interface.h` **removed**. `wifi_interface_t` now defined in `esp_wifi_types_generic.h`.
- Macros `ESP_IF_WIFI_STA` / `ESP_IF_WIFI_AP` **removed** â†’ use `WIFI_IF_STA` / `WIFI_IF_AP` directly.
- Auth modes `WIFI_AUTH_WPA3_EXT_PSK`, `WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE` removed â†’ `WIFI_AUTH_WPA3_PSK`.
- Bandwidth enums `WIFI_BW_HT20`/`WIFI_BW_HT40` removed â†’ `WIFI_BW20`/`WIFI_BW40`.
- `esp_wifi_set_ant*`/`get_ant*` moved to `esp_phy` component as `esp_phy_set_ant*` etc.

**Behavior change:** "If the Wi-Fi driver has already been initialized by `esp_wifi_init`, calling `esp_wifi_init` again will not reinitialize the driver and will return **`ESP_ERR_INVALID_STATE`** instead of `ESP_OK`." â†’ don't wrap a re-init in `ESP_ERROR_CHECK` inside a reconnect path.

**Kconfig prefix migration** (`components\esp_wifi\sdkconfig.rename`): all `CONFIG_ESP32_WIFI_*` â†’ `CONFIG_ESP_WIFI_*` (e.g. `CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM` â†’ `CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM`), and `CONFIG_WPA_*` â†’ `CONFIG_ESP_WIFI_*`. Old names still auto-translate via the rename file, but write new names in `sdkconfig.defaults`.

### esp_netif change

`esp_netif_next()` **removed** â†’ `esp_netif_next_unsafe()` (inside `esp_netif_tcpip_exec()`) or `esp_netif_find_if()` with a predicate. Only matters if we enumerate interfaces.

lwIP TCP/IP thread renamed **"tiT" â†’ "tcpip"** â€” update any log filters or stack-size lookups keyed on the old name.

---

## 11. NVS (component `nvs_flash`)

Headers `nvs_flash.h` / `nvs.h`. API **unchanged** â€” all the usual signatures verified present:

```c
esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_init_partition(const char *partition_label);
esp_err_t nvs_flash_erase(void);
esp_err_t nvs_open(const char* namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);
esp_err_t nvs_set_str (nvs_handle_t handle, const char* key, const char* value);
esp_err_t nvs_get_str (nvs_handle_t handle, const char* key, char* out_value, size_t* length);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char* key, const void* value, size_t length);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value, size_t* length);
esp_err_t nvs_set_u8/u16/u32/u64/i8/i16/i32/i64(...);
esp_err_t nvs_get_u8/...(...);
esp_err_t nvs_erase_key(nvs_handle_t handle, const char* key);
esp_err_t nvs_erase_all(nvs_handle_t handle);
esp_err_t nvs_commit(nvs_handle_t handle);
esp_err_t nvs_get_stats(const char *part_name, nvs_stats_t *nvs_stats);
```

New in v6 (additive, safe to ignore): `nvs_flash_init_partition_bdl()` (block-device-layer backend) and `nvs_purge_all()`. `nvs_flash` now has `REQUIRES esp_partition esp_blockdev` â€” a new `esp_blockdev` component, but transitively handled.

Standard idiom still correct:
```c
esp_err_t err = nvs_flash_init();
if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
}
ESP_ERROR_CHECK(err);
```

---

## 12. Logging (`ESP_LOGx`)

`components\log\Kconfig`: `CONFIG_LOG_VERSION` choice, **default `LOG_VERSION_1`** (V2 is opt-in â€” V2 gives smaller binaries and runtime format config at some stack cost). Levels via `Kconfig.level` (`CONFIG_LOG_DEFAULT_LEVEL_*`, `CONFIG_LOG_MAXIMUM_LEVEL_*`).

ðŸŸ¡ v6 removals (`system.rst`):
- `esp_log_buffer_hex()` **removed** â†’ use macro **`ESP_LOG_BUFFER_HEX`**
- `esp_log_buffer_char()` **removed** â†’ use macro **`ESP_LOG_BUFFER_CHAR`**
- Header `esp_log_internal.h` **removed** â†’ `esp_log_buffer.h`

`ESP_LOGE/W/I/D/V` themselves unchanged. `log` is a common component â€” never list it in REQUIRES.

---

## 13. Other v6 changes that touch this project

| Area | Change | Impact |
|---|---|---|
| **libc** | Default switched **Newlib â†’ Picolibc**. Breaking: per-task `stdin`/`stdout`/`stderr` redefinition no longer possible (globals, POSIX behavior). `CONFIG_LIBC_PICOLIBC_NEWLIB_COMPATIBILITY` on by default. `newlib` component renamed **`esp_libc`**. All `CONFIG_NEWLIB_*` â†’ `CONFIG_LIBC_*` (e.g. `CONFIG_NEWLIB_NANO_FORMAT` â†’ `CONFIG_LIBC_NEWLIB_NANO_FORMAT`) | Low risk; smaller binary, less stack |
| **libc** | `CONFIG_COMPILER_ASSERT_NDEBUG_EVALUATE` default â†’ **n**: with `NDEBUG`, `assert(expr)` no longer evaluates `expr` | âš ï¸ Any `assert(foo() == 0)` with side effects silently stops running |
| **esp_common** | `EXT_RAM_ATTR` **removed** â†’ `EXT_RAM_BSS_ATTR` | Only if using PSRAM |
| **esp_common** | `esp_fault.h` moved `esp_hw_support` â†’ `esp_common` | |
| **hw_support** | `soc_memory_types.h` removed â†’ `esp_memory_utils.h`; `intr_types.h` removed â†’ `esp_intr_types.h` | |
| **Core dump** | Binary format dropped (ELF only); CRC32 checksum dropped (SHA256 only) | `CONFIG_ESP_COREDUMP_DATA_FORMAT_BIN` no longer supported |
| **Bootloader** | `-O0` (`CONFIG_BOOTLOADER_COMPILER_OPTIMIZATION_NONE`) removed â†’ use `-Og` | |
| **SPI flash** | `esp_spi_flash.h` removed â†’ `spi_flash_mmap.h`; `CONFIG_SPI_FLASH_ROM_DRIVER_PATCH` removed | |
| **Kconfig** | Now **esp-idf-kconfig v3** â€” syntax changes if we write custom `Kconfig.projbuild` for device config | Consult the esp-idf-kconfig v2â†’v3 migration guide |
| **Tools** | Minimum **Python 3.10** (3.9 dropped); minimum **CMake 3.22.1**; `idf.py efuse*` now requires `--port` | |
| **Toolchain** | **GCC 15.1.0** | New warnings, now errors by default |
| **mbedTLS** | Upgraded to **v4.0**, PSA Crypto is now the primary interface | Relevant only for MQTT-over-TLS; do not poke mbedTLS internals |
| **ESP-TLS** | wolfSSL support **removed**; `CONFIG_ESP_TLS_USING_WOLFSSL`, `CONFIG_ESP_DEBUG_WOLFSSL`, `CONFIG_ESP_TLS_OCSP_CHECKALL` gone | mbedTLS is the default; no action if we never used wolfSSL |
| **VFS** | TERMIOS **disabled by default** (`CONFIG_VFS_SUPPORT_TERMIOS`); `esp_vfs_console` renamed **`esp_stdio`** (now a common component) | |
| **App trace** | `app_trace` â†’ **`esp_trace`** in REQUIRES; config menu moved to `Component config > ESP Trace Configuration`; must now explicitly enable transport | Only if we use apptrace |
| **Heap** | `MALLOC_CAP_EXEC` now **undefined** (compile error) when `CONFIG_ESP_SYSTEM_MEMPROT` is on | |
| **SNTP** | `sntp.h` **removed** â†’ `esp_sntp.h` | If we timestamp readings |
| **Ping** | `esp_ping.h` / `ping.h` removed â†’ `ping/ping_sock.h` | If we add connectivity checks |

---

## 14. Net-new work items this report creates

1. **Fetch `espressif/mqtt` before writing any MQTT code** â€” the header is not on disk, so LWT/QoS/retain/auth struct fields cannot be verified from this tree yet. Requires network.
2. **Decide cJSON vs. hand-rolled discovery payload** â€” the built-in `json` component is gone.
3. **Audit every `REQUIRES`** â€” `mqtt`, `json`, `driver`, `newlib`, `app_trace` are all wrong for v6.
4. **Audit the WiFi disconnect handler** for the three removed `WIFI_REASON_*` codes.
5. **Audit periodic loops for `vTaskDelayUntil`** â†’ `xTaskDelayUntil`.
6. **Add explicit FreeRTOS includes** to every module (drivers and `esp_event.h` no longer pull them in).
7. **Set `CONFIG_ESPTOOLPY_FLASHSIZE_8MB`** â€” default is 2 MB.
8. **Set `CONFIG_FREERTOS_HZ=1000`** â€” default is 100 Hz (10 ms ticks).
9. **Set `CONFIG_ESP_TASK_WDT_PANIC=y`** â€” default n means a hung task just logs.
10. **Check I2C error handling for `ESP_ERR_INVALID_RESPONSE`** on NACK, not `ESP_ERR_INVALID_STATE`.
11. **Write the SHT3x driver in-house** â€” nothing ships with IDF.
12. **Budget for warnings-as-errors** under GCC 15.