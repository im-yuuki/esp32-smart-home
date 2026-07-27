# v6 component split: esp_driver_gpio/esp_driver_i2c (never the legacy 'driver'
# husk), esp_hw_support for esp_mac.h/esp_random.h, esp_app_format for
# esp_app_desc.h. 'mqtt' is the managed espressif/mqtt component (mesh-example
# pattern: listing it explicitly is valid and clearer). cJSON is auto-injected
# by the component manager -- 'json'/'cjson' must NEVER appear here.
# esp_http_server is the in-tree httpd; dns_server is the vendored project
# component under firmware/components/ (auto-discovered, no registry dep).
# lwip is listed explicitly because portal.c calls getsockname/getpeername
# directly (AP-only gate); esp_http_server keeps lwip PRIVATE, so relying on
# the transitive include path would be luck.
# portal.html is EMBED_TXTFILES (NUL-terminated _binary_portal_html_start/_end).
idf_component_register(
    SRCS "main.c" "app_events.c" "app_config.c" "wifi_manager.c" "mqtt_mgr.c"
         "relay_driver.c" "button_handler.c" "sensor_task.c" "sht31.c" "discovery.c"
         "portal.c" "boot_button.c" "json_guard.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "portal.html"
    PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_timer esp_event
                  esp_wifi esp_netif nvs_flash esp_hw_support esp_app_format mqtt
                  esp_http_server dns_server lwip)
