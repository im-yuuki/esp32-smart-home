# Smart Home Node Firmware (ESP32-S3)

Phase-1 node firmware: 2 relays, 2 push buttons, optional SHT31
temperature/humidity sensor, MQTT with LWT + retained discovery, plus a
recovery SoftAP + captive-portal config UI.

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

## Recovery portal (SoftAP + captive portal)

The node can serve a recovery/provisioning web portal from the device itself.

**Activation triggers**

1. **Unprovisioned boot** — WiFi SSID still empty / the Kconfig placeholder:
   the portal starts immediately (no STA connection attempts).
2. **STA down 180 s** — provisioned but no IP for 180 s cumulative downtime
   (reset only by getting an IP): the portal starts *alongside* continued STA
   retries (APSTA). Once the STA gets an IP the portal stays up for a 60 s
   grace period, then stops and the node returns to pure STA.
3. **BOOT button held 5 s** — GPIO0 on any devkit, at any time: forces the
   portal (stays up until Save & Reboot). A future longer hold is reserved
   for factory reset (GD3) — not implemented yet.

**Connecting**

- SSID: `SmartHome-Setup-XXXX` (last 4 hex digits of the MAC), **open**
  network, portal at `http://192.168.4.1/`. A DNS hijack + captive-probe
  redirects make phones pop the portal automatically.
- The DNS hijack is a **convenience, never a prerequisite**. If it fails to
  start (for example the port is still held by the previous portal cycle) the
  node logs `captive-portal DNS unavailable ...` and brings the portal up
  anyway — the automatic popup does not appear, but everything that actually
  recovers the node works over `http://192.168.4.1/` typed by hand. The startup
  line reports which mode you are in (`DNS hijack on` / `DNS hijack OFF`).
- **Reachable only from the SoftAP.** During trigger 2 the node is in APSTA
  with a working LAN connection, and the HTTP server necessarily listens on
  every interface (IDF's httpd has no bind-to-netif option). Every `/api/*`
  request — including `/api/login` — must therefore be addressed **to**
  192.168.4.1 *and* come **from** an address inside the SoftAP subnet
  (192.168.4.0/24, which only the node's own DHCP server hands out); anything
  else is answered `403 {"error":"ap_only"}`, so the login cannot be
  brute-forced from the home LAN. Both halves are required — lwIP will accept a
  packet addressed to the AP IP that arrived on the STA link, so the
  destination check alone is not proof of origin. The login page itself and the
  captive-probe redirect stay open (they expose nothing and change nothing).
  The DNS hijack binds the SoftAP address only, so it never answers LAN
  queries. Practical consequence: browse the portal over the
  `SmartHome-Setup-XXXX` WiFi, **not** via the node's LAN IP.
- Login: user `admin`, password `defaultpasswd` (Kconfig
  `CONFIG_SHC_ADMIN_PASS`, seeded to NVS key `admin_pass` on first boot).
  **Change it immediately** — the UI shows a warning banner while the default
  is in use.
- Session: RAM-only cookie (`HttpOnly`, `SameSite=Strict`), dies on reboot.
  The cookie is a **session cookie** — it carries no `Max-Age`, so the browser
  keeps it for as long as the tab lives. Expiry is **idle** expiry and is
  enforced server-side: 10 minutes with no authenticated request. The open
  portal tab polls `/api/status` every 2 s, so an open tab never expires; close
  it (or log out) and the session is gone. Earlier builds also put a 10 min
  `Max-Age` on the cookie, which logged you out mid-typing and cleared the form
  on re-login — that is fixed.

**Features**

- Status: node id, fw version, STA state/IP/SSID, MQTT, sensor, relay states.
- WiFi scan + primary and **backup** AP credentials (after 6 consecutive
  connect failures the node alternates primary↔backup; reboot always starts
  on primary). Scanning never disconnects a live STA link, and a scan-induced
  disconnect does not count toward the backup-AP threshold. An empty scan is a
  normal result ("No networks found"), not an error.
- IP mode: DHCP (default) or static (ip/netmask/gateway/dns) on the STA.
  Static settings are validated before they are stored — a config that parses
  but cannot work is rejected with `400 {"error":"bad_subnet"}`:
  - the netmask must be non-zero and contiguous (`255.255.255.0` yes,
    `0.0.0.0` and `255.0.255.0` no);
  - the netmask may not be longer than `/30` (`255.255.255.252`) — a `/31` or
    `/32` leaves no room for a host plus a gateway;
  - the gateway must be inside the host's own subnet
    (`ip & netmask == gateway & netmask`) and must not be the node's own
    address;
  - the IP may not be the subnet's network or broadcast address.

  On top of that, **while the node is currently associated to a WiFi network**
  a static address on a *different* network than the live one is rejected with
  `400 {"error":"wrong_subnet"}` — the proposed `ip & netmask` is compared
  against the STA's live `ip & netmask`. This is the typo that actually bricks
  nodes, and it passes every arithmetic check because it is internally
  consistent. When the STA is *not* connected the check is skipped: the user
  may legitimately be reconfiguring the node for a different network.

  This matters more than it looks: a valid-but-wrong static IP (say
  `192.168.0.50/24 gw 192.168.0.1` on a `192.168.1.0/24` LAN) still produces an
  `IP_EVENT_STA_GOT_IP`, and that event is the only thing that resets the 180 s
  downtime clock. The node would be on WiFi, unreachable, MQTT dead, with
  trigger 2 permanently disarmed — recoverable only by the 5 s BOOT hold.
  A blank DNS field still means "use the gateway".
- The Save button stays disabled until the portal has successfully read the
  current config, and says so. `Save & Reboot` posts the whole form (an empty
  Backup SSID *clears* the backup AP, and the IP mode is always sent), so
  saving a form that never loaded would quietly wipe settings.
- MQTT broker URI / username / password.
- Emergency relay toggles — same single-writer queue as buttons/MQTT, work
  with WiFi and MQTT fully down (published as source `button`).
- Change admin password (immediate), Save & Reboot (config to NVS, then a
  clean reboot with a graceful MQTT `offline`).

While the portal is inactive, the HTTP server, DNS server and the AP netif do
not exist (attack-surface minimization). Request bodies are capped at 1 KB, and
every parse of JSON that came from outside the node — portal request bodies
*and* MQTT payloads from the broker — is preceded by a nesting-depth scan
(`json_guard.c`, cap 8; `CONFIG_CJSON_NESTING_LIMIT` is pinned to 16 as a
second line of defense), so no remote peer can recurse cJSON through a task
stack.

Config schema is v2; v1 stores are migrated in place. Seeding and migration
only ever write keys that do **not** exist yet — and "does not exist" means
exactly `ESP_ERR_NVS_NOT_FOUND`, not "the read failed for any reason", so a
flaky flash read can never be mistaken for an absent key and overwrite what you
configured. A failed seed or migration is **non-fatal**: the node logs the
error, keeps the compiled-in defaults for whatever is missing, serves relays
normally, and retries on the next boot — with no risk of that retry reverting
the admin password to the compiled-in default.

Boot never panics on a config problem. An unreadable `cfg_ver` is treated like
a first boot (seed the missing keys, touch nothing that exists) instead of
being returned into `ESP_ERROR_CHECK`, and a failing `esp_read_mac` falls back
to `node_id = esp32s3-000000` with a loud error rather than a reset loop. Such
a node is broken and its MQTT topics will collide with any other node in the
same state — but it still switches the lights and still serves the portal.

## Build verification without hardware

```powershell
idf.py build      # must be green with zero warning escape hatches enabled
idf.py size       # app must fit the 3M factory partition
```

`dependencies.lock` is committed — it pins the exact mirror versions for
reproducible offline builds. `managed_components/`, `build/`, `sdkconfig`
are gitignored.
