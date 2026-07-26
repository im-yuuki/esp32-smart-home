#include "relay_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#include "app_config.h"
#include "mqtt_mgr.h"

static const char *TAG = "relay";

#define RELAY_QUEUE_LEN  8
#define RELAY_TASK_STACK 4096   // bytes (IDF, not words)
#define RELAY_TASK_PRIO  10     // > mqtt task (5) so local latency stays low

static QueueHandle_t s_queue;
static bool s_state[2];
static relay_source_t s_last_source[2] = { RELAY_SRC_BOOT, RELAY_SRC_BOOT };

static void relay_task(void *arg)
{
    (void)arg;
    const app_config_t *cfg = app_config_get();
    const uint32_t active = cfg->relay_active_level ? 1u : 0u;
    const uint32_t inactive = active ? 0u : 1u;

    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    relay_cmd_t cmd;
    for (;;) {
        if (xQueueReceive(s_queue, &cmd, pdMS_TO_TICKS(2000)) == pdTRUE) {
            if (cmd.channel < 1 || cmd.channel > cfg->relay_count) {
                ESP_LOGW(TAG, "invalid relay channel %u", (unsigned)cmd.channel);
            } else {
                uint8_t idx = cmd.channel - 1;
                bool new_state = (cmd.op == RELAY_OP_TOGGLE) ? !s_state[idx] : cmd.state;

                // 1. ACT (only place a relay GPIO is written)
                gpio_set_level((gpio_num_t)cfg->relay_gpio[idx], new_state ? active : inactive);
                s_state[idx] = new_state;
                s_last_source[idx] = cmd.source;

                // 2. PERSIST (nvs_commit inside)
                esp_err_t err = app_config_save_relay_state(cmd.channel, new_state);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "persist relay %u failed: %s", (unsigned)cmd.channel, esp_err_to_name(err));
                }

                // 3. REPORT (no-op while MQTT is offline; connect sequence resyncs)
                mqtt_mgr_publish_relay_state(cmd.channel, new_state, cmd.source);

                ESP_LOGI(TAG, "relay %u -> %s (src=%d)", (unsigned)cmd.channel, new_state ? "ON" : "OFF",
                         (int)cmd.source);
            }
        }
        esp_task_wdt_reset();
    }
}

esp_err_t relay_driver_init(void)
{
    const app_config_t *cfg = app_config_get();

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << cfg->relay_gpio[0]) | (1ULL << cfg->relay_gpio[1]),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }

    // Drive both relays to the inactive level BEFORE applying power-on
    // behavior so they can never glitch on.
    const uint32_t inactive = cfg->relay_active_level ? 0u : 1u;
    for (uint8_t i = 0; i < cfg->relay_count; i++) {
        gpio_set_level((gpio_num_t)cfg->relay_gpio[i], inactive);
        s_state[i] = false;
    }

    s_queue = xQueueCreate(RELAY_QUEUE_LEN, sizeof(relay_cmd_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(relay_task, "relay", RELAY_TASK_STACK, NULL, RELAY_TASK_PRIO, NULL) != pdPASS) {
        return ESP_FAIL;
    }

    // Power-on behavior flows through the same command path as everything else.
    for (uint8_t ch = 1; ch <= cfg->relay_count; ch++) {
        bool on = false;
        switch (cfg->poweron) {
        case POWERON_ON:
            on = true;
            break;
        case POWERON_OFF:
            on = false;
            break;
        case POWERON_RESTORE:
        default:
            if (app_config_load_relay_state(ch, &on) != ESP_OK) {
                on = false;  // never saved -> off
            }
            break;
        }
        relay_cmd_t cmd = { .channel = ch, .op = RELAY_OP_SET, .state = on, .source = RELAY_SRC_BOOT };
        relay_driver_send_cmd(&cmd);
    }

    ESP_LOGI(TAG, "ready: %u relays on GPIO %u/%u, %s, poweron=%d", (unsigned)cfg->relay_count,
             (unsigned)cfg->relay_gpio[0], (unsigned)cfg->relay_gpio[1],
             cfg->relay_active_level ? "active-high" : "active-low", (int)cfg->poweron);
    return ESP_OK;
}

bool relay_driver_send_cmd(const relay_cmd_t *cmd)
{
    if (s_queue == NULL || cmd == NULL) {
        return false;
    }
    if (xQueueSend(s_queue, cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "relay cmd queue full, dropping cmd for ch %u", (unsigned)cmd->channel);
        return false;
    }
    return true;
}

bool relay_driver_get_state(uint8_t ch)
{
    if (ch < 1 || ch > 2) {
        return false;
    }
    return s_state[ch - 1];
}

relay_source_t relay_driver_get_last_source(uint8_t ch)
{
    if (ch < 1 || ch > 2) {
        return RELAY_SRC_BOOT;
    }
    return s_last_source[ch - 1];
}
