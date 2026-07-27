// Recovery/provisioning captive portal (SoftAP + DNS hijack + embedded UI).
// portal_init() only registers event handlers and creates timers -- httpd,
// DNS server and the AP netif exist ONLY while the portal is active.
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t portal_init(void);
bool      portal_is_active(void);

#ifdef __cplusplus
}
#endif
