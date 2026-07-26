#include "sensor_task.h"

#include <inttypes.h>
#include <stdio.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_task_wdt.h"

#include "app_config.h"
#include "mqtt_mgr.h"
#include "sensor_driver.h"
#include "sht31.h"

static const char *TAG = "sensor";

#define SENSOR_TASK_STACK   4096  // bytes
#define SENSOR_TASK_PRIO    4
#define MAX_CONSEC_FAILURES 3

static sensor_driver_t *s_drv;
static bool s_present;

bool sensor_present(void)
{
    return s_present;
}

static void sensor_task_fn(void *arg)
{
    (void)arg;
    const app_config_t *cfg = app_config_get();
    int consec_failures = 0;

    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    for (;;) {
        float t = 0.0f, h = 0.0f;
        esp_err_t err = s_drv->read(s_drv, &t, &h);
        if (err == ESP_OK) {
            consec_failures = 0;
            if (t < -20.0f || t > 80.0f || h < 0.0f || h > 100.0f) {
                ESP_LOGW(TAG, "discarding out-of-range sample T=%.1f RH=%.1f", (double)t, (double)h);
            } else {
                char payload[96];
                snprintf(payload, sizeof payload,
                         "{\"temperature\":%.1f,\"humidity\":%.1f,\"ts\":%lld}",
                         (double)t, (double)h, (long long)time(NULL));
                // QoS 0, not retained; silently skipped while MQTT offline.
                mqtt_mgr_publish("sensor/state", payload, 0, false);
            }
        } else {
            consec_failures++;
            if (consec_failures == MAX_CONSEC_FAILURES) {
                // NACK (ESP_ERR_INVALID_RESPONSE in v6) => sensor vanished.
                // Keep trying at the normal period; self-heals if re-attached.
                ESP_LOGE(TAG, "%d consecutive read failures (last: %s) -- sensor detached?",
                         consec_failures, esp_err_to_name(err));
            } else {
                ESP_LOGW(TAG, "read failed: %s", esp_err_to_name(err));
            }
        }

        // Wait the interval in 1 s slices, feeding the TWDT each slice
        // (30 s period vs 10 s TWDT timeout).
        for (uint32_t i = 0; i < cfg->sensor_interval_s; i++) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_task_wdt_reset();
        }
    }
}

esp_err_t sensor_task_start(void)
{
    const app_config_t *cfg = app_config_get();
    s_drv = sht31_get_driver(cfg->i2c_sda, cfg->i2c_scl);

    esp_err_t err = s_drv->init(s_drv);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C init failed (%s) - sensor disabled, discovery will omit sensor capability",
                 esp_err_to_name(err));
        s_drv->deinit(s_drv);
        s_present = false;
        return ESP_OK;
    }

    err = s_drv->probe(s_drv);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SHT31 not found (%s) - sensor disabled, discovery will omit sensor capability",
                 esp_err_to_name(err));
        s_drv->deinit(s_drv);
        s_present = false;
        return ESP_OK;  // node continues normally -- current acceptance configuration
    }

    s_present = true;
    ESP_LOGI(TAG, "%s present, publishing every %" PRIu32 " s", s_drv->model, cfg->sensor_interval_s);
    if (xTaskCreate(sensor_task_fn, "sensor", SENSOR_TASK_STACK, NULL, SENSOR_TASK_PRIO, NULL) != pdPASS) {
        s_present = false;  // keep discovery honest
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
