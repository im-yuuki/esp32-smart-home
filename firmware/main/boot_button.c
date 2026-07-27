#include "boot_button.h"

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "app_events.h"

static const char *TAG = "boot_btn";

// BOOT strapping pin: a plain button to GND on every S3 devkit; input-only
// use after boot is safe, so no Kconfig option (uniform hardware).
#define BOOT_BTN_GPIO GPIO_NUM_0
#define DEBOUNCE_US   50000  // 50 ms

// Hold-duration table: one one-shot timer per slot, all armed on a debounced
// press, all stopped on release. GD3 adds factory reset as a second, longer
// slot here without touching the detection logic.
typedef struct {
    uint64_t hold_us;
    void (*fire)(void);
} hold_slot_t;

static void hold_portal_fire(void);

static const hold_slot_t s_holds[] = {
    { 5ULL * 1000 * 1000, hold_portal_fire },
    // GD3: add { 15ULL * 1000 * 1000, hold_factory_reset_fire } here
};
#define NUM_HOLDS (sizeof s_holds / sizeof s_holds[0])

static esp_timer_handle_t s_hold_timers[NUM_HOLDS];
static esp_timer_handle_t s_debounce;

static void hold_portal_fire(void)
{
    ESP_LOGI(TAG, "BOOT held 5 s -- requesting portal");
    int32_t r = PORTAL_REASON_MANUAL;
    esp_event_post(APP_EVENT, APP_EVENT_PORTAL_START_REQ, &r, sizeof r, 0);
}

// ISR body -- same IRAM-safety rationale as button_handler.c: the ISR service
// is installed with flags 0 (no ESP_INTR_FLAG_IRAM) and esp_timer stop/
// start_once are IRAM-safe by default. No delay, no queue, no logging here.
static void boot_isr(void *arg)
{
    (void)arg;
    if (gpio_get_level(BOOT_BTN_GPIO) == 0) {  // press edge
        esp_timer_stop(s_debounce);
        esp_timer_start_once(s_debounce, DEBOUNCE_US);
    } else {  // release edge: abort the debounce and every pending hold
        esp_timer_stop(s_debounce);
        for (size_t i = 0; i < NUM_HOLDS; i++) {
            esp_timer_stop(s_hold_timers[i]);
        }
    }
}

// esp_timer task: press confirmed after 50 ms -- arm every hold slot.
static void debounce_cb(void *arg)
{
    (void)arg;
    if (gpio_get_level(BOOT_BTN_GPIO) == 0) {
        for (size_t i = 0; i < NUM_HOLDS; i++) {
            esp_timer_stop(s_hold_timers[i]);
            esp_timer_start_once(s_hold_timers[i], s_holds[i].hold_us);
        }
    }
}

// esp_timer task: hold duration elapsed -- fire if the button is still down.
static void hold_cb(void *arg)
{
    size_t idx = (size_t)(uintptr_t)arg;
    if (gpio_get_level(BOOT_BTN_GPIO) == 0) {
        s_holds[idx].fire();
    }
}

esp_err_t boot_button_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BOOT_BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,  // both edges: the release must cancel the hold
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {  // INVALID_STATE = already installed
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(err));
        return err;
    }

    const esp_timer_create_args_t dargs = {
        .callback = debounce_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "boot_debounce",
    };
    err = esp_timer_create(&dargs, &s_debounce);
    if (err != ESP_OK) {
        return err;
    }
    for (size_t i = 0; i < NUM_HOLDS; i++) {
        const esp_timer_create_args_t hargs = {
            .callback = hold_cb,
            .arg = (void *)(uintptr_t)i,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "boot_hold",
        };
        err = esp_timer_create(&hargs, &s_hold_timers[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    err = gpio_isr_handler_add(BOOT_BTN_GPIO, boot_isr, NULL);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "BOOT button (GPIO0): hold 5 s for the recovery portal");
    return ESP_OK;
}
