#include "mqtt_mgr.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "cJSON.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"  // esp_restart
#include "esp_timer.h"
#include "mqtt_client.h"

#include "app_config.h"
#include "app_events.h"
#include "discovery.h"
#include "json_guard.h"
#include "relay_driver.h"

static const char *TAG = "mqtt";

#define TOPIC_MAX      160
#define REBOOT_DELAY_US 1500000  // let the graceful offline publish flush

static esp_mqtt_client_handle_t s_client;
static char s_base_topic[96];    // "home/{room}/{node_id}/"
static char s_status_topic[112]; // base + "status" -- must outlive the client (LWT points at it)
static volatile bool s_connected;
static bool s_started;
static esp_timer_handle_t s_reboot_timer;

static const char *src_str(relay_source_t s)
{
    switch (s) {
    case RELAY_SRC_MQTT:
        return "mqtt";
    case RELAY_SRC_BUTTON:
        return "button";
    case RELAY_SRC_BOOT:
    default:
        return "boot";
    }
}

static void reboot_cb(void *arg)
{
    (void)arg;
    esp_restart();
}

static void handle_relay_set(int ch, const esp_mqtt_event_handle_t event)
{
    const app_config_t *cfg = app_config_get();
    if (ch < 1 || ch > (int)cfg->relay_count) {
        ESP_LOGW(TAG, "relay set for invalid channel %d", ch);
        return;
    }

    // Depth guard before cJSON, same reason as in the portal: the parser
    // recurses per nesting level and this task has a 6 KB stack against a 2 KB
    // receive buffer. Broker payloads are as untrusted as portal bodies.
    if (!json_depth_ok_n(event->data, (size_t)event->data_len)) {
        ESP_LOGW(TAG, "relay %d set: over-nested JSON dropped (%d bytes)", ch, event->data_len);
        return;
    }
    cJSON *root = cJSON_ParseWithLength(event->data, (size_t)event->data_len);
    if (root == NULL) {
        ESP_LOGW(TAG, "relay %d set: invalid JSON (%.*s)", ch, event->data_len, event->data);
        return;
    }

    bool valid = false;
    bool on = false;
    const cJSON *state = cJSON_GetObjectItemCaseSensitive(root, "state");
    if (cJSON_IsString(state) && state->valuestring != NULL) {
        if (strcmp(state->valuestring, "ON") == 0) {
            valid = true;
            on = true;
        } else if (strcmp(state->valuestring, "OFF") == 0) {
            valid = true;
            on = false;
        }
    }

    if (valid) {
        relay_cmd_t cmd = { .channel = (uint8_t)ch, .op = RELAY_OP_SET, .state = on, .source = RELAY_SRC_MQTT };
        relay_driver_send_cmd(&cmd);
    } else {
        ESP_LOGW(TAG, "relay %d set: payload must be {\"state\":\"ON\"|\"OFF\"}", ch);
    }
    cJSON_Delete(root);
}

static void handle_cmd(const esp_mqtt_event_handle_t event)
{
    if (!json_depth_ok_n(event->data, (size_t)event->data_len)) {
        ESP_LOGW(TAG, "cmd: over-nested JSON dropped (%d bytes)", event->data_len);
        return;
    }
    cJSON *root = cJSON_ParseWithLength(event->data, (size_t)event->data_len);
    if (root == NULL) {
        ESP_LOGW(TAG, "cmd: invalid JSON (%.*s)", event->data_len, event->data);
        return;
    }
    const cJSON *action = cJSON_GetObjectItemCaseSensitive(root, "action");
    if (cJSON_IsString(action) && action->valuestring != NULL && strcmp(action->valuestring, "reboot") == 0) {
        ESP_LOGI(TAG, "reboot command received -- graceful offline, restart in 1.5 s");
        mqtt_mgr_publish("status", "offline", 1, true);  // graceful (overrides LWT race)
        esp_timer_start_once(s_reboot_timer, REBOOT_DELAY_US);  // never esp_restart() in the handler
    } else {
        ESP_LOGW(TAG, "cmd: unknown action (%.*s)", event->data_len, event->data);
    }
    cJSON_Delete(root);
}

// MQTT_EVENT_DATA dispatch. event->topic is NOT NUL-terminated and only valid
// on the first fragment -- caller already enforced total_data_len == data_len.
static void handle_data(const esp_mqtt_event_handle_t event)
{
    const int base_len = (int)strlen(s_base_topic);
    if (event->topic_len <= base_len || strncmp(event->topic, s_base_topic, (size_t)base_len) != 0) {
        ESP_LOGW(TAG, "unexpected topic %.*s", event->topic_len, event->topic);
        return;
    }
    const char *sub = event->topic + base_len;
    const int sub_len = event->topic_len - base_len;

    if (sub_len == 11 && strncmp(sub, "relay/", 6) == 0 && strncmp(sub + 7, "/set", 4) == 0
        && sub[6] >= '0' && sub[6] <= '9') {
        handle_relay_set(sub[6] - '0', event);
    } else if (sub_len == 3 && strncmp(sub, "cmd", 3) == 0) {
        handle_cmd(event);
    } else {
        ESP_LOGW(TAG, "unhandled topic %.*s", event->topic_len, event->topic);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    const esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED: {
        ESP_LOGI(TAG, "connected -- running connect sequence");
        s_connected = true;  // before the sequence so relay-state publishes pass

        // Spec's exact connect sequence, in order:
        // 1. status online (QoS1 retained)
        mqtt_mgr_publish("status", "online", 1, true);
        // 2. discovery (QoS1 retained)
        discovery_publish();
        // 3. every relay state (QoS1 retained) -- the offline-resync mechanism
        const app_config_t *cfg = app_config_get();
        for (uint8_t ch = 1; ch <= cfg->relay_count; ch++) {
            mqtt_mgr_publish_relay_state(ch, relay_driver_get_state(ch), relay_driver_get_last_source(ch));
        }
        // 4. subscriptions (subscribe_single: esp_mqtt_client_subscribe is a _Generic macro)
        char topic[TOPIC_MAX];
        snprintf(topic, sizeof topic, "%srelay/+/set", s_base_topic);
        if (esp_mqtt_client_subscribe_single(s_client, topic, 1) < 0) {
            ESP_LOGE(TAG, "subscribe %s failed", topic);
        }
        snprintf(topic, sizeof topic, "%scmd", s_base_topic);
        if (esp_mqtt_client_subscribe_single(s_client, topic, 1) < 0) {
            ESP_LOGE(TAG, "subscribe %s failed", topic);
        }
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        // esp-mqtt reconnects on its own (.network.reconnect_timeout_ms) -- no manual restart.
        s_connected = false;
        ESP_LOGW(TAG, "disconnected (esp-mqtt will auto-reconnect)");
        break;
    case MQTT_EVENT_DATA:
        if (event->total_data_len != event->data_len) {
            ESP_LOGW(TAG, "fragmented message dropped (%d of %d bytes)", event->data_len, event->total_data_len);
            break;
        }
        handle_data(event);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "subscribed, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_ERROR: {
        const esp_mqtt_error_codes_t *eh = event->error_handle;
        if (eh != NULL) {
            // connect_return_code makes bad-credential debugging trivial
            // (e.g. MQTT_CONNECTION_REFUSE_BAD_USERNAME).
            ESP_LOGE(TAG, "error_type=%d connect_return_code=%d sock_errno=%d",
                     (int)eh->error_type, (int)eh->connect_return_code, eh->esp_transport_sock_errno);
        }
        break;
    }
    default:
        break;
    }
}

static void app_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)data;
    if (id == APP_EVENT_WIFI_GOT_IP) {
        if (!s_started) {
            s_started = true;
            esp_err_t err = esp_mqtt_client_start(s_client);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "client start failed: %s", esp_err_to_name(err));
                s_started = false;
            }
        } else {
            // Shortcut esp-mqtt's retry wait now that WiFi is back.
            esp_mqtt_client_reconnect(s_client);
        }
    }
    // APP_EVENT_WIFI_LOST: nothing -- esp-mqtt notices the broken socket itself.
}

esp_err_t mqtt_mgr_start(void)
{
    const app_config_t *cfg = app_config_get();

    snprintf(s_base_topic, sizeof s_base_topic, "home/%s/%s/", cfg->room, cfg->node_id);
    snprintf(s_status_topic, sizeof s_status_topic, "%sstatus", s_base_topic);

    // Every field below verified against the mirrored espressif/mqtt v1.0.0 header.
    const esp_mqtt_client_config_t mc = {
        .broker.address.uri = cfg->mqtt_uri,
        .credentials.username = cfg->mqtt_user,
        .credentials.client_id = cfg->node_id,
        .credentials.authentication.password = cfg->mqtt_pass,
        .session.last_will = {
            .topic = s_status_topic,
            .msg = "offline",
            .msg_len = 0,  // 0 => strlen(msg)
            .qos = 1,
            .retain = 1,
        },
        .session.keepalive = 30,               // LWT fires <= ~45 s -> inside the 90 s budget
        .network.reconnect_timeout_ms = 5000,  // esp-mqtt owns MQTT-level reconnect
        .network.timeout_ms = 10000,
        .task = { .priority = 5, .stack_size = 6144 },  // explicit: independent of component Kconfig
        .buffer = { .size = 2048 },            // discovery JSON ~400 B; headroom
        .outbox.limit = 8192,                  // bound RAM while broker unreachable
    };

    s_client = esp_mqtt_client_init(&mc);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
        return ESP_FAIL;
    }

    const esp_timer_create_args_t targs = {
        .callback = reboot_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "mqtt_reboot",
    };
    esp_err_t err = esp_timer_create(&targs, &s_reboot_timer);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_handler_instance_register(APP_EVENT, ESP_EVENT_ANY_ID, app_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "configured for %s (client_id=%s, base=%s)", cfg->mqtt_uri, cfg->node_id, s_base_topic);
    return ESP_OK;
}

bool mqtt_mgr_is_connected(void)
{
    return s_connected;
}

int mqtt_mgr_publish(const char *subtopic, const char *payload, int qos, bool retain)
{
    if (s_client == NULL) {
        return -1;
    }
    if (!s_connected && qos == 0) {
        return -1;  // don't grow the outbox with stale QoS0 (sensor) data
    }
    char topic[TOPIC_MAX];
    snprintf(topic, sizeof topic, "%s%s", s_base_topic, subtopic);
    // len=0 => strlen(payload); store=true => outbox-backed, non-blocking.
    return esp_mqtt_client_enqueue(s_client, topic, payload, 0, qos, retain ? 1 : 0, true);
}

void mqtt_mgr_publish_relay_state(uint8_t ch, bool state, relay_source_t src)
{
    if (!s_connected) {
        return;  // resync happens in the connect sequence
    }
    char sub[24];
    char payload[64];
    snprintf(sub, sizeof sub, "relay/%u/state", (unsigned)ch);
    snprintf(payload, sizeof payload, "{\"state\":\"%s\",\"source\":\"%s\"}",
             state ? "ON" : "OFF", src_str(src));
    mqtt_mgr_publish(sub, payload, 1, true);
}
