#include "button_handler.h"

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "app_config.h"
#include "relay_driver.h"

static const char *TAG = "button";

#define NUM_BUTTONS  2
#define DEBOUNCE_US  50000  // 50 ms

static esp_timer_handle_t s_debounce[NUM_BUTTONS];

// ISR body -- complete. gpio_install_isr_service(0) means no ESP_INTR_FLAG_IRAM,
// so no IRAM_ATTR needed; esp_timer_stop/start_once are IRAM-safe by default
// (CONFIG_ESP_TIMER_IN_IRAM=y). stop-then-start makes the 50 ms window
// retriggerable across bounces (start_once on a running timer would return
// ESP_ERR_INVALID_STATE). No delay, no queue, no logging, no MQTT here.
static void button_isr(void *arg)
{
    uint32_t ch = (uint32_t)(uintptr_t)arg;  // 1-based channel
    esp_timer_handle_t t = s_debounce[ch - 1];
    esp_timer_stop(t);  // ok to fail when not running
    esp_timer_start_once(t, DEBOUNCE_US);
}

// Runs in the esp_timer task (normal task context): resample the pin; if
// still low after 50 ms it is a real press.
static void debounce_cb(void *arg)
{
    uint32_t ch = (uint32_t)(uintptr_t)arg;
    const app_config_t *cfg = app_config_get();
    if (gpio_get_level((gpio_num_t)cfg->button_gpio[ch - 1]) == 0) {
        relay_cmd_t cmd = { .channel = (uint8_t)ch, .op = RELAY_OP_TOGGLE, .source = RELAY_SRC_BUTTON };
        relay_driver_send_cmd(&cmd);  // plain xQueueSend inside -- task context
    }
}

esp_err_t button_handler_init(void)
{
    const app_config_t *cfg = app_config_get();

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << cfg->button_gpio[0]) | (1ULL << cfg->button_gpio[1]),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,  // press = falling edge with pull-up
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

    for (uint32_t ch = 1; ch <= NUM_BUTTONS; ch++) {
        const esp_timer_create_args_t targs = {
            .callback = debounce_cb,
            .arg = (void *)(uintptr_t)ch,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "btn_debounce",
        };
        err = esp_timer_create(&targs, &s_debounce[ch - 1]);
        if (err != ESP_OK) {
            return err;
        }
        err = gpio_isr_handler_add((gpio_num_t)cfg->button_gpio[ch - 1], button_isr, (void *)(uintptr_t)ch);
        if (err != ESP_OK) {
            return err;
        }
    }

    ESP_LOGI(TAG, "buttons on GPIO %u/%u (pull-up, falling edge, 50 ms debounce)",
             (unsigned)cfg->button_gpio[0], (unsigned)cfg->button_gpio[1]);
    return ESP_OK;
}
