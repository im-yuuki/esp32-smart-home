# v6 component split: esp_driver_gpio/esp_driver_i2c (never the legacy 'driver'
# husk), esp_hw_support for esp_mac.h, esp_app_format for esp_app_desc.h.
# 'mqtt' is the managed espressif/mqtt component (mesh-example pattern: listing
# it explicitly is valid and clearer). cJSON is auto-injected by the component
# manager -- 'json'/'cjson' must NEVER appear here.
idf_component_register(
    SRCS "main.c" "app_events.c" "app_config.c" "wifi_manager.c" "mqtt_mgr.c"
         "relay_driver.c" "button_handler.c" "sensor_task.c" "sht31.c" "discovery.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_timer esp_event
                  esp_wifi esp_netif nvs_flash esp_hw_support esp_app_format mqtt)
