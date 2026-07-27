// Smart-home node -- app_main: sequential init, then returns.
//
// Order matters: relays restore before network so local control never waits
// on WiFi; sensor probe precedes the first discovery publish (which happens
// on MQTT connect). Failures in steps 4-6 are logged but non-fatal (a node
// with a broken sensor must still serve relays); 1-3 are fatal.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "boot_button.h"
#include "button_handler.h"
#include "mqtt_mgr.h"
#include "portal.h"
#include "relay_driver.h"
#include "sensor_task.h"
#include "wifi_manager.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    // 1. NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase (%s)", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "NVS ready");

    // 2. Config (seed-once, load, node_id)
    ESP_ERROR_CHECK(app_config_init());

    // 3. Default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 4. Relays (power-on behavior applied here, source "boot")
    err = relay_driver_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "relay_driver_init failed: %s (continuing)", esp_err_to_name(err));
    }

    // 5. Buttons
    err = button_handler_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "button_handler_init failed: %s (continuing)", esp_err_to_name(err));
    }

    // 5b. BOOT button (GPIO0): hold 5 s to force the recovery portal
    err = boot_button_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "boot_button_init failed: %s (continuing)", esp_err_to_name(err));
    }

    // 6. Sensor (absent => warn + no task)
    err = sensor_task_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sensor_task_start failed: %s (continuing)", esp_err_to_name(err));
    }

    // 6b. Portal (event handlers only; SoftAP/httpd/DNS start on demand).
    // Must precede wifi_manager_start: the unprovisioned-boot
    // PORTAL_START_REQ posted there needs the portal handler registered.
    err = portal_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "portal_init failed: %s (continuing)", esp_err_to_name(err));
    }

    // 7. WiFi
    err = wifi_manager_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wifi_manager_start failed: %s", esp_err_to_name(err));
    }

    // 8. MQTT (client starts on first APP_EVENT_WIFI_GOT_IP)
    err = mqtt_mgr_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mqtt_mgr_start failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "init complete: node %s, fw %s",
             app_config_get()->node_id, esp_app_get_description()->version);
    // 9. return -- main task ends; the system runs on tasks/events.
}
