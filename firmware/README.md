# Smart Home Node Firmware (ESP32-S3)

Phase-1 node firmware: 2 relays, 2 push buttons, optional SHT31
temperature/humidity sensor, MQTT with LWT + retained discovery.

- Framework: **ESP-IDF v6.0.2** (local tree `C:\Espressif\esp\v6.0.2\esp-idf`)
- Target: `esp32s3`, custom partition table (nvs 24K / phy 4K / factory 3M), 4 MB flash assumed
- Managed components: `espressif/mqtt ^1.0.0`, `espressif/cjson ^1.7.19`
  (both resolve **offline** from the mirror at `C:\Espressif\tools` — see below)

## Environment activation

Two equivalent methods; use one per shell session.

### A. Interactive (PowerShell profile — recommended)

```powershell
. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1
cd C:\Users\Izuki\Projects\SmartHomeController\firmware
idf.py set-target esp32s3     # once; regenerates sdkconfig from sdkconfig.defaults
idf.py build
```

The profile sets `IDF_PATH`, the Python venv, tool paths and — critically —
`IDF_COMPONENT_LOCAL_STORAGE_URL=file://C:\Espressif\tools`, which makes the
component manager resolve `espressif/mqtt` and `espressif/cjson` from the
local mirror without network access. `idf.py` only exists as an alias inside
the session that dot-sourced the profile.

### B. Scripted (CI-style, no profile)

```powershell
$env:IDF_COMPONENT_LOCAL_STORAGE_URL = "file://C:\Espressif\tools"
& "C:\Espressif\tools\python\v6.0.2\venv\Scripts\python.exe" `
  "C:\Espressif\esp\v6.0.2\esp-idf\tools\idf.py" `
  -C C:\Users\Izuki\Projects\SmartHomeController\firmware set-target esp32s3
& "C:\Espressif\tools\python\v6.0.2\venv\Scripts\python.exe" `
  "C:\Espressif\esp\v6.0.2\esp-idf\tools\idf.py" `
  -C C:\Users\Izuki\Projects\SmartHomeController\firmware build
```

If the component manager unexpectedly tries the network and fails, the missing
`IDF_COMPONENT_LOCAL_STORAGE_URL` env var is the fix.

## Configuration (menuconfig)

```powershell
idf.py menuconfig    # -> "Smart Home Node Configuration"
```

`CONFIG_SHC_*` options: WiFi SSID/password, MQTT broker URI/username/password,
room slug (default `phong-khach`), relay GPIOs 4/5 + names + active level,
power-on behavior (restore/off/on), button GPIOs 6/7, I2C SDA 8 / SCL 9,
sensor interval (default 30 s).

### Seeding semantics — read this before reflashing

Kconfig values are **compiled-in defaults only**. On first boot (NVS key
`cfg_ver` absent) they are seeded into NVS namespace `shc_cfg` once;
afterwards **NVS is the single source of truth**. Reflashing with different
menuconfig values does *not* overwrite an existing config. To force a
re-seed during development:

```powershell
idf.py -p COM4 erase-flash
idf.py -p COM4 flash
```

(`erase-flash` also clears saved relay states in namespace `shc_state`.)

## Flash & monitor

```powershell
idf.py -p COM4 flash monitor    # Ctrl+] to exit monitor
```

## Behavior summary

- `node_id` = `esp32s3-` + last 6 hex digits of the STA MAC (e.g. `esp32s3-a1b2c3`).
- Topic base: `home/{room}/{node_id}/`
  - `status` — retained; `online` on connect, `offline` via LWT (keepalive 30 s
    ⇒ LWT ≤ ~45 s) or gracefully before a commanded reboot
  - `discovery` — retained JSON; sensor capability **omitted when no SHT31 is
    detected at boot** (node stays fully functional without one)
  - `relay/{ch}/state` — retained `{"state":"ON|OFF","source":"mqtt|button|boot"}`
  - `relay/{ch}/set` — subscribed; `{"state":"ON"|"OFF"}`
  - `cmd` — subscribed; `{"action":"reboot"}`
  - `sensor/state` — QoS 0 `{"temperature":..,"humidity":..,"ts":..}` every 30 s
    (only when the sensor is present)
- Buttons toggle relays fully offline; the MQTT connect sequence republishes
  all relay states, so the server resyncs automatically after reconnect.
- Power-on behavior `restore` re-applies NVS-saved relay states *before* WiFi.

## Build verification without hardware

```powershell
idf.py build      # must be green with zero warning escape hatches enabled
idf.py size       # app must fit the 3M factory partition
```

`dependencies.lock` is committed — it pins the exact mirror versions for
reproducible offline builds. `managed_components/`, `build/`, `sdkconfig`
are gitignored.
