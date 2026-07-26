# ADR-0001: ESP-IDF v6.0.2 instead of the roadmap's v5.x

**Status:** accepted (2026-07-26)

## Context
The roadmap specifies ESP-IDF v5.2+. The development machine has v6.0.2 installed (EIM install at `C:\Espressif`), and v6 is the current supported line. v6 introduces breaking changes vs the v5-written spec — verified against the local tree (see `docs/design/idf-v6-ground-truth-*.md`).

## Decision
Target ESP-IDF v6.0.2. Consequences absorbed into the firmware design:
- `esp-mqtt` and `cJSON` are managed registry components (`espressif/mqtt` 1.0.0, `espressif/cjson` 1.7.19), declared in `main/idf_component.yml`; both resolve **offline** from the local mirror (`IDF_COMPONENT_LOCAL_STORAGE_URL=file://C:\Espressif\tools`). `dependencies.lock` is committed.
- Driver component split: `esp_driver_gpio` / `esp_driver_i2c` (new `driver/i2c_master.h` API); the legacy `driver` component is EOL and must not be referenced.
- Local module renamed `mqtt_client.c/.h` → `mqtt_mgr.c/.h` to avoid shadowing the managed component's public header.
- Default compiler warnings are errors (GCC 15); FreeRTOS headers require explicit includes.

## Consequences
Firmware code is not copy-paste compatible with v5.x tutorials. The full pitfalls checklist lives in `docs/design/firmware-plan.md` §5.
