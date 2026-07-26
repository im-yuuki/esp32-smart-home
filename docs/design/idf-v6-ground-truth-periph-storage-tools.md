# FOCUS AREA 3 â€” Ground Truth from Local ESP-IDF v6.0.2 Tree

Verified against `C:\Espressif\esp\v6.0.2\esp-idf` (version.cmake: MAJOR 6 / MINOR 0 / PATCH 2).

---

## 0. TL;DR â€” v5.x spec deltas that will bite you

| # | Change | Impact |
|---|---|---|
| 1 | **`json` (cJSON) component REMOVED from core IDF.** Now `espressif/cjson` via component manager. | Your JSON discovery payload **cannot** `REQUIRES json`. Needs `main/idf_component.yml`. |
| 2 | **HAL split into per-peripheral components.** `hal/gpio_types.h` now lives in `components/esp_hal_gpio/`, `hal/i2c_types.h` in `components/esp_hal_i2c/`. | Header include paths unchanged (`hal/gpio_types.h`), but component graph differs. |
| 3 | **Driver public headers no longer include FreeRTOS headers.** | You must explicitly `#include "freertos/FreeRTOS.h"` / `"freertos/task.h"` before using `vTaskDelay`/`pdMS_TO_TICKS` in files that only included `driver/i2c_master.h`. |
| 4 | `esp_event.h` no longer pulls in FreeRTOS headers either â€” add `#include "freertos/queue.h"` / `"freertos/semphr.h"` explicitly. | |
| 5 | **I2C NACK now returns `ESP_ERR_INVALID_RESPONSE`** (was `ESP_ERR_INVALID_STATE` in v5.x) from `i2c_master_transmit`, `i2c_master_multi_buffer_transmit`, `i2c_master_transmit_receive`, `i2c_master_execute_defined_operations`. | Your SHT31 error handling must match on the new code. |
| 6 | Legacy `driver/i2c.h` is **End-of-Life** in v6.0, removal in v7.0. New API is mandatory going forward. | |
| 7 | `gpio_uninstall_isr_service()` now returns `esp_err_t` (was `void`). | |
| 8 | **Linker orphan sections are now an ERROR** (`CONFIG_COMPILER_ORPHAN_SECTIONS`). | New builds can fail on hand-written asm/sections. |
| 9 | FreeRTOS moved out of IRAM by default; `CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH` removed, replaced by new `CONFIG_FREERTOS_IN_IRAM`. | Affects ISR-safe design. |
| 10 | Removed FreeRTOS compat funcs: `vTaskDelayUntil` (â†’ `xTaskDelayUntil`), `xQueueGenericReceive`, `xTaskGetAffinity` (â†’ `xTaskGetCoreID`). | |
| 11 | Minimum CMake is now 3.22.1; local install ships CMake **4.0.3**. | |
| 12 | esp-idf-kconfig **v3** â€” Kconfig syntax changes if you write custom `Kconfig.projbuild`. | |

---

## 1. I2C master (new driver)

**Header (confirmed path):** `C:\Espressif\esp\v6.0.2\esp-idf\components\esp_driver_i2c\include\driver\i2c_master.h`
Include as: `#include "driver/i2c_master.h"`
Supporting types: `C:\Espressif\esp\v6.0.2\esp-idf\components\esp_driver_i2c\include\driver\i2c_types.h` and `C:\Espressif\esp\v6.0.2\esp-idf\components\esp_hal_i2c\include\hal\i2c_types.h` (**moved** â€” was `components/hal/include/hal/i2c_types.h` in v5.x).

### Config structs (verbatim field names)

```c
typedef struct {
    i2c_port_num_t i2c_port;              // -1 for auto-select
    gpio_num_t sda_io_num;
    gpio_num_t scl_io_num;
    union {
        i2c_clock_source_t clk_source;
#if SOC_LP_I2C_SUPPORTED
        lp_i2c_clock_source_t lp_source_clk;
#endif
    };
    uint8_t glitch_ignore_cnt;            // typical 7 (unit: I2C module clock cycle)
    int intr_priority;                    // 0 => driver picks (1,2,3)
    size_t trans_queue_depth;             // async only
    struct {
        uint32_t enable_internal_pullup: 1;
        uint32_t allow_pd:               1;   // backup/restore regs across sleep
    } flags;
} i2c_master_bus_config_t;

typedef struct {
    i2c_addr_bit_len_t dev_addr_length;   // I2C_ADDR_BIT_LEN_7
    uint16_t device_address;              // raw 7/10-bit, no R/W bit
    uint32_t scl_speed_hz;
    uint32_t scl_wait_us;                 // 0 => driver default reg value
    struct {
        uint32_t disable_ack_check:      1;
    } flags;
} i2c_device_config_t;
```

`#define I2C_DEVICE_ADDRESS_NOT_USED (0xffff)`

Handles: `i2c_master_bus_handle_t` (opaque `struct i2c_master_bus_t *`), `i2c_master_dev_handle_t` (opaque `struct i2c_master_dev_t *`).
`typedef int i2c_port_num_t;`

**v5.x delta:** `allow_pd` flag is present in this tree (added post-5.2). `scl_wait_us` and `flags.disable_ack_check` are both present in `i2c_device_config_t`.

### Function signatures (exact, lines 122â€“365)

```c
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *bus_config,
                             i2c_master_bus_handle_t *ret_bus_handle);

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus_handle,
                                    const i2c_device_config_t *dev_config,
                                    i2c_master_dev_handle_t *ret_handle);

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t i2c_dev,
                              const uint8_t *write_buffer, size_t write_size,
                              int xfer_timeout_ms);

esp_err_t i2c_master_receive(i2c_master_dev_handle_t i2c_dev,
                             uint8_t *read_buffer, size_t read_size,
                             int xfer_timeout_ms);

esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t i2c_dev,
                                      const uint8_t *write_buffer, size_t write_size,
                                      uint8_t *read_buffer, size_t read_size,
                                      int xfer_timeout_ms);

esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus_handle,
                           uint16_t address, int xfer_timeout_ms);

esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus_handle);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t handle);
esp_err_t i2c_master_bus_reset(i2c_master_bus_handle_t bus_handle);
esp_err_t i2c_master_bus_wait_all_done(i2c_master_bus_handle_t bus_handle, int timeout_ms);
esp_err_t i2c_master_get_bus_handle(i2c_port_num_t port_num, i2c_master_bus_handle_t *ret_handle);
esp_err_t i2c_master_multi_buffer_transmit(i2c_master_dev_handle_t i2c_dev,
                                           i2c_master_transmit_multi_buffer_info_t *buffer_info_array,
                                           size_t array_size, int xfer_timeout_ms);
esp_err_t i2c_master_execute_defined_operations(i2c_master_dev_handle_t i2c_dev,
                                                i2c_operation_job_t *i2c_operation,
                                                size_t operation_list_num, int xfer_timeout_ms);
esp_err_t i2c_master_register_event_callbacks(i2c_master_dev_handle_t i2c_dev,
                                              const i2c_master_event_callbacks_t *cbs, void *user_data);
esp_err_t i2c_master_device_change_address(i2c_master_dev_handle_t i2c_dev,
                                           uint16_t new_device_address, int timeout_ms);
```

`xfer_timeout_ms == -1` means wait forever.

### Enums for ESP32-S3

- `i2c_port_t`: `I2C_NUM_0`, `I2C_NUM_1` (guarded `SOC_HP_I2C_NUM >= 2`), `I2C_NUM_MAX`. No LP I2C on S3.
- `i2c_addr_bit_len_t`: `I2C_ADDR_BIT_LEN_7 = 0`, `I2C_ADDR_BIT_LEN_10 = 1` (10-bit guarded by `SOC_I2C_SUPPORT_10BIT_ADDR`).
- Clock source (`C:\Espressif\esp\v6.0.2\esp-idf\components\soc\esp32s3\include\soc\clk_tree_defs.h:345`): `soc_periph_i2c_clk_src_t` = `I2C_CLK_SRC_XTAL`, `I2C_CLK_SRC_RC_FAST`, `I2C_CLK_SRC_DEFAULT = SOC_MOD_CLK_XTAL`.

### Kconfig options (`components/esp_driver_i2c/Kconfig`)

Menu: `"ESP-Driver:I2C Configurations"` (depends on `SOC_I2C_SUPPORTED`)
- `CONFIG_I2C_ISR_IRAM_SAFE`
- `CONFIG_I2C_ENABLE_DEBUG_LOG`
- `CONFIG_I2C_MASTER_ISR_HANDLER_IN_IRAM`

### SHT31 pattern â€” CONFIRMED SUPPORTED

`i2c_master_transmit()` and `i2c_master_receive()` both take the **same** `i2c_master_dev_handle_t`, and are independent, blocking, complete transactions (each generates its own STARTâ€¦STOP). So the SHT31 single-shot sequence works exactly as needed:

1. `i2c_master_transmit(dev, cmd2, 2, timeout_ms)` â€” write 2-byte measurement command (e.g. `{0x24, 0x00}`)
2. `vTaskDelay(pdMS_TO_TICKS(20))` â€” ~15 ms conversion wait (round up; 15 ms is not a tick multiple at the default 100 Hz tick, so use 20 ms or raise `CONFIG_FREERTOS_HZ`)
3. `i2c_master_receive(dev, buf, 6, timeout_ms)` â€” read T_MSB,T_LSB,T_CRC,RH_MSB,RH_LSB,RH_CRC

Note `i2c_master_transmit_receive()` is **not** what you want for SHT31 single-shot â€” it issues a repeated-START with no gap, which does not allow the 15 ms conversion delay. Use it only for SHT31 register reads that respond immediately (e.g. read-status `0xF32D`), or if you use SHT31 periodic/clock-stretch mode.

**Important for step 2:** `driver/i2c_master.h` in v6 does **not** transitively include FreeRTOS headers. Add `#include "freertos/FreeRTOS.h"` and `#include "freertos/task.h"` in your SHT31 source file.

Also use `i2c_master_probe(bus, 0x44, 100)` at init to detect the SHT31 (`0x44` default, `0x45` alt). It returns `ESP_ERR_NOT_FOUND` on NACK and `ESP_ERR_TIMEOUT` if pull-ups are missing.

### Local examples

Examples live under `C:\Espressif\esp\v6.0.2\esp-idf\examples\peripherals\i2c\`: `i2c_basic`, `i2c_eeprom`, `i2c_slave_network_sensor`, `i2c_tools`, `i2c_u8g2`.

**`i2c_basic`** (`...\i2c_basic\main\i2c_basic_example_main.c`) â€” closest match to your needs. MPU9250 over I2C:
- Bus config uses `.i2c_port = I2C_NUM_0`, `.clk_source = I2C_CLK_SRC_DEFAULT`, `.glitch_ignore_cnt = 7`, `.flags.enable_internal_pullup = true`.
- Device config uses `.dev_addr_length = I2C_ADDR_BIT_LEN_7`, `.device_address`, `.scl_speed_hz`.
- Register read helper is a one-liner over `i2c_master_transmit_receive(dev, &reg_addr, 1, data, len, 1000)`.
- Register write is `i2c_master_transmit(dev, write_buf, 2, 1000)`.
- Teardown: `i2c_master_bus_rm_device()` then `i2c_del_master_bus()`.
- Its `main/CMakeLists.txt` is bare (`SRCS` + `INCLUDE_DIRS "."`) because `main` implicitly requires everything.

**`i2c_eeprom`** â€” shows a driver packaged as a separate component; its `main/CMakeLists.txt` uses `PRIV_REQUIRES esp_driver_i2c i2c_eeprom`. This is the pattern to copy for a `sht31` component of your own. It also uses `.i2c_port = -1` (auto-select).

**`i2c_tools`** (`...\i2c_tools\main\cmd_i2ctools.c:107`) â€” the only in-tree user of `i2c_master_probe`.

### Component name for `REQUIRES`

`esp_driver_i2c`. Its own registration (`components/esp_driver_i2c/CMakeLists.txt`) is:
`REQUIRES esp_hal_i2c`, `PRIV_REQUIRES esp_driver_gpio esp_pm esp_ringbuf`.

---

## 2. GPIO

**Header:** `C:\Espressif\esp\v6.0.2\esp-idf\components\esp_driver_gpio\include\driver\gpio.h`
**Types:** `C:\Espressif\esp\v6.0.2\esp-idf\components\esp_hal_gpio\include\hal\gpio_types.h` (**moved in v6** from `components/hal/include/hal/gpio_types.h`).

### `gpio_config_t` (verbatim, gpio.h:34â€“44)

```c
typedef struct {
    uint64_t pin_bit_mask;          // set with bit mask, each bit maps to a GPIO
    gpio_mode_t mode;
    gpio_pullup_t pull_up_en;
    gpio_pulldown_t pull_down_en;
    gpio_int_type_t intr_type;
#if SOC_GPIO_SUPPORT_PIN_HYS_FILTER
    gpio_hys_ctrl_mode_t hys_ctrl_mode;
#endif
} gpio_config_t;
```

Unchanged from v5.x. Note `gpio_config()` "always overwrite all the current IO configurations".

### Enum values

- `gpio_mode_t`: `GPIO_MODE_DISABLE`, `GPIO_MODE_INPUT`, `GPIO_MODE_OUTPUT`, `GPIO_MODE_OUTPUT_OD`, `GPIO_MODE_INPUT_OUTPUT_OD`, `GPIO_MODE_INPUT_OUTPUT`
- `gpio_pullup_t`: `GPIO_PULLUP_DISABLE = 0x0`, `GPIO_PULLUP_ENABLE = 0x1`
- `gpio_pulldown_t`: `GPIO_PULLDOWN_DISABLE = 0x0`, `GPIO_PULLDOWN_ENABLE = 0x1`
- `gpio_int_type_t`: `GPIO_INTR_DISABLE=0`, `GPIO_INTR_POSEDGE=1`, `GPIO_INTR_NEGEDGE=2`, `GPIO_INTR_ANYEDGE=3`, `GPIO_INTR_LOW_LEVEL=4`, `GPIO_INTR_HIGH_LEVEL=5`, `GPIO_INTR_MAX`
- `typedef void (*gpio_isr_t)(void *arg);`

### Signatures

```c
esp_err_t gpio_config(const gpio_config_t *pGPIOConfig);
esp_err_t gpio_reset_pin(gpio_num_t gpio_num);
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level);
int       gpio_get_level(gpio_num_t gpio_num);
esp_err_t gpio_set_direction(gpio_num_t gpio_num, gpio_mode_t mode);
esp_err_t gpio_set_intr_type(gpio_num_t gpio_num, gpio_int_type_t intr_type);
esp_err_t gpio_intr_enable(gpio_num_t gpio_num);
esp_err_t gpio_intr_disable(gpio_num_t gpio_num);
esp_err_t gpio_pullup_en(gpio_num_t gpio_num);
esp_err_t gpio_pullup_dis(gpio_num_t gpio_num);
esp_err_t gpio_pulldown_en(gpio_num_t gpio_num);
esp_err_t gpio_pulldown_dis(gpio_num_t gpio_num);
esp_err_t gpio_set_pull_mode(gpio_num_t gpio_num, gpio_pull_mode_t pull);

esp_err_t gpio_install_isr_service(int intr_alloc_flags);
esp_err_t gpio_uninstall_isr_service(void);              // <-- v6: now returns esp_err_t (was void)
esp_err_t gpio_isr_handler_add(gpio_num_t gpio_num, gpio_isr_t isr_handler, void *args);
esp_err_t gpio_isr_handler_remove(gpio_num_t gpio_num);
esp_err_t gpio_isr_register(void (*fn)(void *), void *arg, int intr_alloc_flags, gpio_isr_handle_t *handle);
```

New in this tree (not in v5.x): `gpio_input_enable()`, `gpio_output_enable()`, `gpio_output_disable()`, `gpio_od_enable()`, `gpio_od_disable()`, `gpio_get_io_config(gpio_num_t, gpio_io_config_t *)`, `gpio_dump_io_configuration(FILE *, uint64_t)`.

**Internal pull-up:** fully supported â€” set `.pull_up_en = GPIO_PULLUP_ENABLE` in `gpio_config_t`, or call `gpio_pullup_en()`. For your 2 push buttons, use `GPIO_MODE_INPUT` + `GPIO_PULLUP_ENABLE` + `GPIO_INTR_NEGEDGE` (or `GPIO_INTR_ANYEDGE` if you want press+release).

**IRAM ISR notes (verbatim from `gpio_isr_handler_add` doc):** "The pin ISR handlers no longer need to be declared with `IRAM_ATTR`, unless you pass the `ESP_INTR_FLAG_IRAM` flag when allocating the ISR in `gpio_install_isr_service()`." Also: per-pin handlers run on a smaller stack than a global handler due to the extra indirection (stack size is `CONFIG_ESP_SYSTEM_..._ISR_STACK_SIZE` in menuconfig).

Recommendation for your debounce ISR: call `gpio_install_isr_service(0)` (no `ESP_INTR_FLAG_IRAM`). Then your handler need not be `IRAM_ATTR`, and you may freely call `esp_timer_start_once` / `xQueueSendFromISR` from it.

`gpio_install_isr_service()` is incompatible with `gpio_isr_register()` â€” pick one. Use the service (per-pin handlers).

### Kconfig
`components/esp_driver_gpio/Kconfig`, menu `"ESP-Driver:GPIO Configurations"`, single option: `CONFIG_GPIO_CTRL_FUNC_IN_IRAM`.

### Component name
`esp_driver_gpio`. Registers as `REQUIRES esp_hal_gpio`, `PRIV_REQUIRES esp_pm`.
**Do not** use `driver` â€” the legacy `driver` component in v6 no longer re-exports GPIO/SPI/UART/etc. and is deprecated.

---

## 3. esp_timer (50 ms debounce)

**Header:** `C:\Espressif\esp\v6.0.2\esp-idf\components\esp_timer\include\esp_timer.h`

```c
typedef struct esp_timer* esp_timer_handle_t;
typedef void (*esp_timer_cb_t)(void* arg);

typedef enum {
    ESP_TIMER_TASK,
#if CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD || __DOXYGEN__
    ESP_TIMER_ISR,
#endif
    ESP_TIMER_MAX,
} esp_timer_dispatch_t;

typedef struct {
    esp_timer_cb_t callback;
    void* arg;
    esp_timer_dispatch_t dispatch_method;
    const char* name;
    bool skip_unhandled_events;
} esp_timer_create_args_t;

esp_err_t esp_timer_create(const esp_timer_create_args_t* create_args,
                           esp_timer_handle_t* out_handle);
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period);
esp_err_t esp_timer_restart(esp_timer_handle_t timer, uint64_t timeout_us);
esp_err_t esp_timer_stop(esp_timer_handle_t timer);
esp_err_t esp_timer_delete(esp_timer_handle_t timer);
bool      esp_timer_is_active(esp_timer_handle_t timer);
int64_t   esp_timer_get_time(void);
```

CONFIRMED â€” all signatures as your spec expects. 50 ms debounce = `esp_timer_start_once(h, 50000)`.

**Practical notes for the debounce design:**
- `esp_timer_start_once()` returns `ESP_ERR_INVALID_STATE` if the timer is **already running**. For a "retriggerable" debounce (restart the window on each bounce edge), use **`esp_timer_restart(h, 50000)`**, or `esp_timer_stop()` then `esp_timer_start_once()`. `esp_timer_restart` is the cleaner primitive and exists in this tree.
- `esp_timer_start_once`, `esp_timer_stop`, `esp_timer_restart` are all marked `ESP_TIMER_IRAM_ATTR` in `components/esp_timer/src/esp_timer.c`, which resolves to `IRAM_ATTR` when `CONFIG_ESP_TIMER_IN_IRAM` is set. **`CONFIG_ESP_TIMER_IN_IRAM` defaults to `y`** (`components/esp_timer/Kconfig:3-5`) â€” so calling these from a GPIO ISR is safe out of the box. This Kconfig name (`ESP_TIMER_IN_IRAM`) is a v6 rename; do not reference the older v5 IRAM knobs.
- With `dispatch_method = ESP_TIMER_TASK` (default, and what you want), the callback runs on the esp_timer task, so it can safely `xQueueSend`, log with `ESP_LOGx`, etc.
- `ESP_TIMER_ISR` dispatch requires `CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD` and forbids blocking / `portYIELD_FROM_ISR()` â€” use `esp_timer_isr_dispatch_need_yield()` instead. Not needed for a 50 ms debounce.
- esp_timer task stack: `CONFIG_ESP_TIMER_TASK_STACK_SIZE`; affinity: `CONFIG_ESP_TIMER_TASK_AFFINITY`.

**Component name:** `esp_timer`.

---

## 4. NVS

**Headers:** `C:\Espressif\esp\v6.0.2\esp-idf\components\nvs_flash\include\nvs_flash.h` and `...\include\nvs.h`

```c
// nvs_flash.h
esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_erase(void);
esp_err_t nvs_flash_init_partition(const char *partition_label);
esp_err_t nvs_flash_erase_partition(const char *part_name);
esp_err_t nvs_flash_deinit(void);

// nvs.h
typedef uint32_t nvs_handle_t;
esp_err_t nvs_open(const char* namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);
esp_err_t nvs_set_str (nvs_handle_t handle, const char* key, const char* value);
esp_err_t nvs_set_u8  (nvs_handle_t handle, const char* key, uint8_t value);
esp_err_t nvs_set_i8  (nvs_handle_t handle, const char* key, int8_t value);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char* key, const void* value, size_t length);
esp_err_t nvs_get_str (nvs_handle_t handle, const char* key, char* out_value, size_t* length);
esp_err_t nvs_get_u8  (nvs_handle_t handle, const char* key, uint8_t* out_value);
esp_err_t nvs_get_i8  (nvs_handle_t handle, const char* key, int8_t* out_value);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value, size_t* length);
esp_err_t nvs_find_key(nvs_handle_t handle, const char* key, nvs_type_t* out_type);
esp_err_t nvs_erase_key(nvs_handle_t handle, const char* key);
esp_err_t nvs_erase_all(nvs_handle_t handle);
esp_err_t nvs_commit(nvs_handle_t handle);
void      nvs_close(nvs_handle_t handle);   // returns void, not esp_err_t
```

`nvs_open_mode_t` = `NVS_READONLY` / `NVS_READWRITE`.

### Two-call size pattern â€” CONFIRMED (documented verbatim in nvs.h:463-476)

> "To get the size necessary to store the value, call `nvs_get_str` or `nvs_get_blob` with zero `out_value` and non-zero pointer to `length`. Variable pointed to by `length` argument will be set to the required length. For `nvs_get_str`, this length **includes the zero terminator**."

In-header example:
```c
size_t required_size;
nvs_get_str(my_handle, "server_name", NULL, &required_size);
char* server_name = malloc(required_size);
nvs_get_str(my_handle, "server_name", server_name, &required_size);
```

### Standard init idiom (error codes confirmed present in nvs.h)
```c
esp_err_t err = nvs_flash_init();
if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
}
ESP_ERROR_CHECK(err);
```
`ESP_ERR_NVS_NOT_FOUND` (`ESP_ERR_NVS_BASE + 0x02`), `ESP_ERR_NVS_NO_FREE_PAGES` (`+0x0d`), `ESP_ERR_NVS_NEW_VERSION_FOUND` (`+0x10`).

**v6 additions worth knowing:** `nvs_purge_all()`, `nvs_flash_init_partition_bdl()` (block-device-layer), `nvs_entry_find_in_handle()`. Nothing you rely on was removed. `nvs_handle` (untyped) and `nvs_open_mode` remain but are `IDF_DEPRECATED` â€” use the `_t` forms.

**Kconfig:** menu `"NVS"` â€” `CONFIG_NVS_ENCRYPTION`, `CONFIG_NVS_ASSERT_ERROR_CHECK`, `CONFIG_NVS_ALLOCATE_CACHE_IN_SPIRAM`, `CONFIG_NVS_FLASH_VERIFY_ERASE`, `CONFIG_NVS_FLASH_ERASE_ATTEMPTS`, `CONFIG_NVS_LEGACY_DUP_KEYS_COMPATIBILITY`, `CONFIG_NVS_BDL_STACK`, `CONFIG_NVS_COMPATIBLE_PRE_V4_3_ENCRYPTION_FLAG`.

**Component name:** `nvs_flash`.

---

## 5. FreeRTOS

**CONFIRMED â€” `xTaskCreate` stack depth is in BYTES.** From `C:\Espressif\esp\v6.0.2\esp-idf\components\freertos\FreeRTOS-Kernel\include\freertos\task.h:315-316`, verbatim:

> `@param usStackDepth The size of the task stack specified as the NUMBER OF BYTES. Note that this differs from vanilla FreeRTOS.`

Signature (task.h:369-401) â€” note it is a `static inline` wrapper in IDF, not an extern function:
```c
#if ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
static inline BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
                                      const char * const pcName,
                                      const configSTACK_DEPTH_TYPE usStackDepth,
                                      void * const pvParameters,
                                      UBaseType_t uxPriority,
                                      TaskHandle_t * const pxCreatedTask );
#endif
```
It forwards to `xTaskCreatePinnedToCore(..., tskNO_AFFINITY)`. Use `xTaskCreatePinnedToCore()` directly (from `freertos/idf_additions.h`) if you want to pin the MQTT/sensor tasks to a specific core on the dual-core S3.

Queues â€” standard, unchanged:
```c
#define xQueueCreate( uxQueueLength, uxItemSize ) \
        xQueueGenericCreate( ( uxQueueLength ), ( uxItemSize ), ( queueQUEUE_TYPE_BASE ) )   // queue.h:149
#define xQueueSendFromISR( xQueue, pvItemToQueue, pxHigherPriorityTaskWoken ) ...            // queue.h:1133
#define xQueueSendToBackFromISR( ... )                                                        // queue.h:988
BaseType_t xQueueGenericSendFromISR( QueueHandle_t xQueue, ... );                             // queue.h:1203
```
`xQueueSendFromISR` is a macro over `xQueueGenericSendFromISR` â€” usage from your button ISR is standard.

**v6 gotchas:**
- Include `"freertos/FreeRTOS.h"` before `"freertos/task.h"` / `"freertos/queue.h"`, and include them *explicitly* â€” driver and `esp_event.h` headers no longer drag them in.
- Removed: `vTaskDelayUntil` (use `xTaskDelayUntil`), `xQueueGenericReceive`, `xTaskGetAffinity` (use `xTaskGetCoreID`), `xTaskGetIdleTaskHandleForCPU`, `xTaskGetCurrentTaskHandleForCPU`.
- Deprecated: `pxTaskGetStackStart` â†’ `xTaskGetStackStart`.
- Most FreeRTOS functions moved from IRAM to flash by default. New knob: `CONFIG_FREERTOS_IN_IRAM`. `CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH` no longer exists.

**Component name:** `freertos` (it is a common component auto-added to every component, so an explicit `REQUIRES freertos` is generally unnecessary but harmless).

Task WDT header for reference: `C:\Espressif\esp\v6.0.2\esp-idf\components\esp_system\include\esp_task_wdt.h` (component `esp_system`).

---

## 6. LOCAL TOOLCHAIN â€” `C:\Espressif`

`C:\Espressif` top level contains only `esp\` and `tools\`. **There is no `idf_cmd_init.bat`, no `Initialize-Idf.ps1`, and no `.ps1`/`.bat` at the `C:\Espressif` root.** This is an **EIM (ESP-IDF Installation Manager)** style install, not the classic `esp-idf-tools` installer layout.

### `C:\Espressif\tools` (top level, exact listing)

```
Microsoft.v6.0.2.PowerShell_deactivate.ps1
Microsoft.v6.0.2.PowerShell_profile.ps1
ccache
cmake
components
dfu-util
eim_idf.json
esp-clang
esp-clang-libs
esp-rom-elfs
esp32ulp-elf
espidf.constraints.v6.0.txt
idf-exe
ninja
openocd-esp32
python
qemu-riscv32
qemu-xtensa
riscv32-esp-elf
riscv32-esp-elf-gdb
xtensa-esp-elf
xtensa-esp-elf-gdb
```

| Requirement | Present? | Exact path |
|---|---|---|
| Python env dir | YES (**`python`**, not `idf-python`/`python_env`) | `C:\Espressif\tools\python\v6.0.2\venv` (`Scripts\python.exe`) |
| xtensa-esp-elf (esp32s3) | YES | `C:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin` â€” contains `xtensa-esp-elf-gcc.exe` (GCC **15.2.0**) and the `xtensa-esp32s3-elf-*` variant binaries |
| ninja | YES | `C:\Espressif\tools\ninja\1.12.1\` |
| cmake | YES | `C:\Espressif\tools\cmake\4.0.3\bin` |
| esptool | YES (2 ways) | pip pkg `esptool 5.3.1` in venv â†’ `...\venv\Scripts\esptool.exe`; plus `C:\Espressif\esp\v6.0.2\esp-idf\components\esptool_py\esptool\esptool.py` |
| `idf.py` launcher exe | YES | `C:\Espressif\tools\idf-exe\1.0.3\idf.py.exe` |
| `idf.py` script | YES | `C:\Espressif\esp\v6.0.2\esp-idf\tools\idf.py` |
| Also present | riscv32-esp-elf 15.2.0, GDB 17.1 (both archs), OpenOCD v0.12.0-esp32-20260424, ccache 4.12.1, esp-clang 20.1.1, ULP toolchain 2.38, QEMU 9.2.2, dfu-util 0.11 |

### `export.ps1` â€” EXISTS
`C:\Espressif\esp\v6.0.2\esp-idf\export.ps1` (and `export.bat`). It is the generic 20-line wrapper:
```powershell
$idf_exports = python "$idf_path/tools/activate.py" --export
. $idf_exports
```
It calls whatever `python` is first on PATH. On this machine that is `C:\Program Files\Python314\python.exe` (system Python 3.14), **not** the IDF venv. That can work (activate.py bootstraps), but it is the fragile path here.

### RECOMMENDED command â€” use the EIM profile script

```powershell
. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1
```

(dot-sourced, so it modifies the current session). This is the authoritative activation for this install. It:
- Sets `IDF_PATH=C:\Espressif\esp\v6.0.2\esp-idf`, `IDF_TOOLS_PATH=C:\Espressif\tools`, `IDF_PYTHON_ENV_PATH=C:\Espressif\tools\python\v6.0.2\venv`, `ESP_ROM_ELF_DIR`, `OPENOCD_SCRIPTS`, `ESP_IDF_VERSION=6.0`, `IDF_CCACHE_ENABLE=1`, `ESP_CLANG_LIBS_PATH`, and **`IDF_COMPONENT_LOCAL_STORAGE_URL=file://C:\Espressif\tools`**
- Prepends all tool `bin` dirs to `PATH`
- Defines `global:Invoke-idfpy` and aliases **`idf.py`** to it (running `venv\Scripts\python.exe esp-idf\tools\idf.py`)
- Defines `esptool` / `esptool.py` / `espefuse` / `espsecure` / `otatool.py` / `parttool.py` global functions
- Dot-sources `...\venv\Scripts\Activate.ps1`
- Registers `idf.py` tab-completion
- Runs `eim select v6.0.2` if `eim` is on PATH

Useful flags/companions:
- `. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1 -e` â†’ print the env vars/PATH without applying them
- `C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_deactivate.ps1` â†’ revert

**Caveat for a non-interactive/automated shell:** `idf.py` is registered as a **PowerShell alias to a function**, not as a real executable on PATH. It only exists inside the PowerShell session that dot-sourced the profile. It will **not** work from Git Bash / cmd / a fresh child process. For scripted builds use either:
```powershell
& "C:\Espressif\tools\python\v6.0.2\venv\Scripts\python.exe" "C:\Espressif\esp\v6.0.2\esp-idf\tools\idf.py" -B build build
```
or add `C:\Espressif\tools\idf-exe\1.0.3` to PATH and invoke `idf.py.exe` (which needs `IDF_PATH` + `IDF_PYTHON_ENV_PATH` set).

Also note the profile does **not** set `IDF_TARGET`. Run `idf.py set-target esp32s3` once in the project.

---

## 7. Component manager

**AVAILABLE.** In `C:\Espressif\tools\python\v6.0.2\venv\Lib\site-packages\`:

- `idf_component_manager` / `idf_component_manager-3.0.3.dist-info` â†’ **idf-component-manager v3.0.3**
- `idf_component_tools`

So `main/idf_component.yml` dependency resolution works out of the box.

Also in that venv: `esptool 5.3.1`, `esp_idf_kconfig 3.12.0`, `esp_idf_monitor 1.9.0`, `esp_idf_size 2.2.1`, `esp_idf_panic_decoder 1.5.0`, `esp_idf_nvs_partition_gen 0.1.9`, `esp_idf_diag 0.2.0`, `idf_build_apps 2.16.1`, `idf_ci 0.7.1`, `pyserial 3.5`.

### Offline registry mirror â€” important

The profile sets `IDF_COMPONENT_LOCAL_STORAGE_URL=file://C:\Espressif\tools`, and `C:\Espressif\tools\components\` contains a pre-seeded local mirror with namespaces: `atanisoft`, `cherry-embedded`, `esp-qa`, `espressif`, `example`, `joltwallet`, `lvgl`.

**Directly relevant:** `C:\Espressif\tools\components\espressif\cjson\1.7.19~2\espressif__cjson-v1.7.19_2.zip` is cached locally.

### Action required for your JSON discovery payload

The built-in `json` component is **gone** in v6.0 (`docs/en/migration-guides/release-6.x/6.0/protocols.rst:9`). Verified: no `cJSON.h` anywhere under `esp-idf/components`. Create `main/idf_component.yml`:

```yaml
dependencies:
  espressif/cjson: "^1.7.19"
```

The API is unchanged (`#include "cJSON.h"`, `cJSON_Parse`, `cJSON_GetObjectItem`, `cJSON_Delete`, `cJSON_PrintUnformatted`). Version `1.7.19~2` resolves against the local mirror, so this works offline. If you build a non-`main` component that uses cJSON, add `REQUIRES espressif__cjson` (managed components get the `namespace__name` component name).

---

## 8. `idf_component_register` cheat-sheet for this firmware

For `main/CMakeLists.txt` â€” **no REQUIRES needed**; `main` automatically requires all components in the build (`docs/en/api-guides/build-system.rst:599`), and `MINIMAL_BUILD` defaults to `OFF`.

For any **separate** component you write (e.g. a `sht31` or `relay` component), use these names:

| Feature | Component name | Public header |
|---|---|---|
| I2C master (SHT31) | `esp_driver_i2c` | `driver/i2c_master.h` |
| GPIO (relays, buttons) | `esp_driver_gpio` | `driver/gpio.h` |
| One-shot debounce timer | `esp_timer` | `esp_timer.h` |
| NVS config | `nvs_flash` | `nvs_flash.h`, `nvs.h` |
| FreeRTOS tasks/queues | `freertos` (common, implicit) | `freertos/FreeRTOS.h`, `freertos/task.h`, `freertos/queue.h` |
| Task watchdog | `esp_system` (common, implicit) | `esp_task_wdt.h` |
| Logging | `log` (common, implicit) | `esp_log.h` |
| Event loop | `esp_event` | `esp_event.h` (+ explicit `freertos/queue.h`, `freertos/semphr.h`) |
| MQTT | `mqtt` | `mqtt_client.h` |
| WiFi / netif | `esp_wifi`, `esp_netif` | â€” |
| JSON | `espressif__cjson` (managed) | `cJSON.h` |

Example:
```cmake
idf_component_register(SRCS "sht31.c" "relay.c" "button.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_i2c
                       PRIV_REQUIRES esp_driver_gpio esp_timer nvs_flash)
```

Do **not** put `driver` in REQUIRES â€” in v6 it is the deprecated legacy shim and no longer re-exports the `esp_driver_*` components.