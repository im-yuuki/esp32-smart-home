# v6 component split: esp_driver_gpio/esp_driver_i2c (never the legacy 'driver'
# husk), esp_hw_support for esp_mac.h/esp_random.h, esp_app_format for
# esp_app_desc.h. 'mqtt' is the managed espressif/mqtt component (mesh-example
# pattern: listing it explicitly is valid and clearer). cJSON is auto-injected
# by the component manager -- 'json'/'cjson' must NEVER appear here.
# esp_http_server is the in-tree httpd; dns_server is the vendored project
# component under firmware/components/ (auto-discovered, no registry dep).
# lwip is listed explicitly because recovery/portal.c calls getsockname/getpeername
# directly (AP-only gate); esp_http_server keeps lwip PRIVATE, so relying on
# the transitive include path would be luck.
# recovery/portal.html is embedded as NUL-terminated text.
idf_component_register(
    SRCS
        "app/main.c"
        "app/app_events.c"
        "config/app_config.c"
        "connectivity/wifi_manager.c"
        "messaging/mqtt_mgr.c"
        "messaging/discovery.c"
        "hardware/relay_driver.c"
        "hardware/button_handler.c"
        "hardware/boot_button.c"
        "sensors/sensor_task.c"
        "sensors/sht31.c"
        "recovery/portal.c"
        "security/json_guard.c"

    PRIV_INCLUDE_DIRS
        "app"
        "config"
        "connectivity"
        "messaging"
        "hardware"
        "sensors"
        "recovery"
        "security"

    EMBED_TXTFILES
        "recovery/portal.html"

    PRIV_REQUIRES
        esp_driver_gpio
        esp_driver_i2c
        esp_timer
        esp_event
        esp_wifi
        esp_netif
        nvs_flash
        esp_hw_support
        esp_app_format
        mqtt
        esp_http_server
        dns_server
        lwip
)
