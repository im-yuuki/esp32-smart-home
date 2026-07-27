# ADR-0004: Sensor-optional firmware (SHT31 behind a driver interface)

**Status:** accepted (2026-07-26, user decision)

## Context
The roadmap's Phase 1 node includes an SHT31 (or DHT22) temp/humidity sensor. The user has no sensor on hand yet.

## Decision
- A small vtable interface `sensor_driver.h` (`init/probe/read`) decouples the sensor task from the concrete chip; `sht31.c` is the first implementation (new `i2c_master` API, addr 0x44/0x45). A DHT22 driver can slot in later without touching `sensor_task`.
- At boot the sensor is **probed**; when absent the firmware logs a warning, does not start the sensor task, and **omits the sensor capability from the discovery payload**. Everything else works normally.
- Server/UI need no special handling: capability-driven discovery already makes the sensor's presence data-driven.
- Phase 1 acceptance for the sensor chart requires real sensor telemetry when hardware is available (wire SDA=GPIO8, SCL=GPIO9, reboot — the capability auto-appears).

## Consequences
Discovery payloads differ between sensor-present and sensor-absent nodes; the server upsert already reconciles capability sets per node, so a later-attached sensor self-registers.
