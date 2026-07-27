# ADR-0005: On-device recovery SoftAP + captive-portal config UI

**Status:** accepted (2026-07-26, user request)

## Context
The roadmap defers node provisioning to Phase 3 (SoftAP + server-generated credentials). The user asked for an earlier, richer on-device portal: recovery access when WiFi is misconfigured/down, runtime configuration (WiFi incl. backup AP, DHCP/static IP, MQTT server address), and emergency local relay control — all from a phone via captive portal, login-protected.

## Decision
Firmware-embedded portal (esp_http_server + single embedded HTML page + vendored DNS-hijack component from the IDF captive-portal example, patched to fix an upstream UDP-socket leak on stop; license `Unlicense OR CC0-1.0` kept verbatim).

- **Activation:** (a) unprovisioned boot → portal only; (b) provisioned but no IP for 180 s → portal **alongside** STA retries (APSTA), auto-stop 60 s after reconnect; (c) BOOT (GPIO0) held 5 s → manual portal. Portal servers run *only* while active.
- **AP:** open network `SmartHome-Setup-<4 hex MAC>` (GĐ3 naming), 192.168.4.1, DNS hijack + OS captive-probe redirects.
- **Auth:** `admin` + NVS-stored password (default `defaultpasswd`, changeable in the portal, warning banner until changed); RAM-only session cookie (esp_fill_random, 10 min idle), 1 s lockout after failed login. Secrets are never echoed back by the config API.
- **Config:** NVS schema v2 (backup AP, ip_mode + static ip/nm/gw/dns, admin_pass) with an explicit v1→v2 migration that only adds keys.
- **Backup AP:** wifi_manager alternates primary↔backup after every 6 consecutive failed attempts; reboot restarts on primary.
- **Emergency relay control** reuses the single-writer `relay_cmd_q`; portal toggles publish as MQTT source `"button"` — the wire contract (`mqtt|button|boot`) is unchanged.

## Hardening (three adversarial review rounds before merge)
The portal is the node's only attack surface that accepts unauthenticated input, so it went through three review→fix→re-verify rounds. Decisions that are load-bearing and must not be silently undone:

- **JSON input is depth-bounded twice.** cJSON's parser is recursive; unbounded nesting in an unauthenticated login body overflows the HTTP task stack (~64 B/level). Bounded by `CONFIG_CJSON_NESTING_LIMIT=16` *and* a pre-parse scan (`json_guard.c`, cap 8) applied to both portal bodies and broker-supplied MQTT payloads. The scanner's equivalence to cJSON's own string/escape handling was confirmed by differential fuzzing (~460 k cases).
- **The AP-only gate needs the peer check.** lwIP accepts a packet addressed to *any* netif's address regardless of arrival interface (`ip4.c` `NETIF_FOREACH` fallback), so "local address == 192.168.4.1" does **not** prove the request came over the SoftAP. The gate requires local == AP IP **and** peer inside the AP subnet, fail-closed on every error.
- **Portal teardown order is fixed.** `esp_netif_action_stop()` must run *before* the mode change and netif destroy, or the DHCP server's UDP pcb leaks on every portal cycle and the portal becomes permanently unreachable after ~11 cycles.
- **NVS seeding is idempotent and never fatal.** Only `ESP_ERR_NVS_NOT_FOUND` counts as "key absent" — treating any read error as absent could silently revert a user-set admin password to the default. Config failures log and continue: a node with working relays must always boot.
- **The portal is never torn down because the DNS hijack failed** — losing the auto-popup is not a reason to lose the recovery path.
- **Static IP is validated beyond parsing** (contiguous mask ≤ /30, gateway in-subnet, not network/broadcast/self, and — unless the same request also changes SSID — on the network the node is currently joined to), because a well-formed but wrong address still raises `GOT_IP`, which permanently disarms the auto-portal trigger.

## Consequences
- Phase 3's provisioning flow can build on this portal (server-generated MQTT credentials entered via the same UI; BOOT longer-hold factory reset slots into the existing hold-detector table).
- Open AP + default password is a deliberate Phase-1 trade-off for a LAN device; the UI nags until the password is changed, and the portal is not running during normal operation.
- +66 KB app size (1,008,160 B total; 68 % of the 3 MB factory partition still free).

## Known follow-ups (accepted, not blocking)
- No "connected but MQTT never reachable" secondary portal trigger — a wrong-but-reachable broker URI still needs a physical BOOT hold to correct.
- A BOOT-hold-started portal has no absolute lifetime cap; login lockout is a flat 1 s with no escalation.
- Portal teardown briefly blocks the default event loop (worst case ~6 s if a scan is in flight).
- SoftAP uses the IDF default 192.168.4.0/24; a home LAN on the same subnet would collide while the portal is up.
