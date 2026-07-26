#include "discovery.h"

#include "cJSON.h"
#include "esp_app_desc.h"  // v6 home of esp_app_get_description (esp_ota_get_app_description removed)
#include "esp_log.h"

#include "app_config.h"
#include "mqtt_mgr.h"
#include "sensor_task.h"
#include "wifi_manager.h"

static const char *TAG = "discovery";

esp_err_t discovery_publish(void)
{
    const app_config_t *cfg = app_config_get();
    const esp_app_desc_t *app = esp_app_get_description();

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "node_id", cfg->node_id);
    cJSON_AddStringToObject(root, "room", cfg->room);
    cJSON_AddStringToObject(root, "fw_version", app->version);
    cJSON_AddStringToObject(root, "ip", wifi_manager_get_ip_str());

    cJSON *caps = cJSON_AddArrayToObject(root, "capabilities");
    for (uint8_t i = 0; i < cfg->relay_count; i++) {
        cJSON *cap = cJSON_CreateObject();
        cJSON_AddStringToObject(cap, "type", "relay");
        cJSON_AddNumberToObject(cap, "channel", i + 1);
        cJSON_AddStringToObject(cap, "name", cfg->relay_name[i]);
        cJSON_AddItemToArray(caps, cap);
    }
    // Sensor capability only when the probe found one -- the spec's
    // sensor-absent requirement.
    if (sensor_present()) {
        cJSON *cap = cJSON_CreateObject();
        cJSON_AddStringToObject(cap, "type", "sensor");
        cJSON_AddNumberToObject(cap, "channel", 1);
        cJSON_AddStringToObject(cap, "kind", "temperature_humidity");
        cJSON_AddStringToObject(cap, "model", "SHT31");
        cJSON_AddNumberToObject(cap, "interval_s", cfg->sensor_interval_s);
        cJSON_AddItemToArray(caps, cap);
    }

    esp_err_t ret = ESP_OK;
    char *json = cJSON_PrintUnformatted(root);
    if (json != NULL) {
        if (mqtt_mgr_publish("discovery", json, 1, true) < 0) {
            ret = ESP_FAIL;
        } else {
            ESP_LOGI(TAG, "published: %s", json);
        }
        cJSON_free(json);
    } else {
        ret = ESP_ERR_NO_MEM;
    }
    cJSON_Delete(root);
    return ret;
}
