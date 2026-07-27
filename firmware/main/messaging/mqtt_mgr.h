// esp-mqtt wrapper: LWT, connect sequence, enqueue-based publishing.
//
// Module is named mqtt_mgr (NOT mqtt_client): the managed component's public
// header is literally mqtt_client.h. A local wrapper with that name could
// shadow the managed component header through the component include paths.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "relay_driver.h"  // relay_source_t

#ifdef __cplusplus
extern "C" {
#endif

// Build config from app_config, init client, register handlers. The client is
// actually started on the first APP_EVENT_WIFI_GOT_IP.
esp_err_t mqtt_mgr_start(void);

bool mqtt_mgr_is_connected(void);

// topic = "home/{room}/{node_id}/" + subtopic. Non-blocking (esp-mqtt outbox).
// Returns message id (>=0) or -1 (dropped: not connected && qos==0, or error).
int mqtt_mgr_publish(const char *subtopic, const char *payload, int qos, bool retain);

// Publish relay/{ch}/state {"state":"ON|OFF","source":"mqtt|button|boot"},
// QoS 1 retained. No-op while offline (connect sequence resyncs all states).
void mqtt_mgr_publish_relay_state(uint8_t ch, bool state, relay_source_t src);

#ifdef __cplusplus
}
#endif
