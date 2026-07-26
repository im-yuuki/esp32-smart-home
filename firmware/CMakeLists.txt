# Smart-home node firmware (Phase 1) - ESP32-S3, ESP-IDF v6.0.2.
#
# PROJECT_VER must be set BEFORE including project.cmake -- this is the
# documented IDF version mechanism (read back at runtime via
# esp_app_get_description()->version). Do NOT also set CONFIG_APP_PROJECT_VER
# in sdkconfig: the Kconfig pair silently takes precedence.
cmake_minimum_required(VERSION 3.22)
set(PROJECT_VER "1.0.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(smart_home_node)
