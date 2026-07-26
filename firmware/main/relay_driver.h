// Relay actuation path. A single relay_task consuming relay_cmd_q is the ONLY
// writer of relay GPIOs -- MQTT / button / boot commands all flow through it,
// which serializes races by construction and enforces act -> persist -> report.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { RELAY_SRC_MQTT, RELAY_SRC_BUTTON, RELAY_SRC_BOOT } relay_source_t;
typedef enum { RELAY_OP_SET, RELAY_OP_TOGGLE } relay_op_t;

typedef struct {
    uint8_t        channel;   // 1-based
    relay_op_t     op;
    bool           state;     // used when op == RELAY_OP_SET
    relay_source_t source;
} relay_cmd_t;

esp_err_t      relay_driver_init(void);                        // gpio outputs, power-on behavior, queue+task
bool           relay_driver_send_cmd(const relay_cmd_t *cmd);  // xQueueSend, 0 ticks; task context only
bool           relay_driver_get_state(uint8_t ch);             // cached, for discovery/connect-sequence
relay_source_t relay_driver_get_last_source(uint8_t ch);       // last source that set ch (connect-sequence republish)

#ifdef __cplusplus
}
#endif
