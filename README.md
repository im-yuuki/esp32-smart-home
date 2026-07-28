# Smart Home ESP32-S3

Multi-room smart-home system: ESP32-S3 nodes (relays + sensors) connect over WiFi/MQTT to a central server with a realtime web UI. The management server provides accounts, node approval, inherited folder-tree RBAC, visual area maps, bulk controls, audit logs, and per-user realtime events without sending user or permission data to ESP32 nodes.

```
[Vue SPA] ⇄ REST + WebSocket ⇄ [Spring Boot] ⇄ MQTT ⇄ [Mosquitto] ⇄ MQTT ⇄ [ESP32-S3 nodes]
                                      ⇅
                                [PostgreSQL]
```

Nodes never talk to the web UI directly. The backend is the only server-side MQTT client; state topics are the single source of truth (the server never infers state from commands it sent). Unicode display names, folders, semantic device types, tags, and map placements are server-side metadata, so firmware identifiers and MQTT topics remain stable.

## Facility management

- Every approved node belongs to exactly one folder. Folder permissions inherit to descendants.
- Folders can use `OUTDOOR`, `BUILDING`, `FLOOR`, `CORRIDOR`, or `ROOM` map presets.
- Node and capability display names support normalized Unicode and are not overwritten by discovery.
- Relay capabilities can be classified and tagged, then controlled in bulk for the current folder or its subtree.
- Every control request and dispatch result is audited. `AUDIT_VIEW` grants scoped access to logs.
- Existing flat groups are migrated to folders by Flyway V3. A node that belonged to multiple groups receives a dedicated import folder whose generated roles preserve the old effective permissions.

## Layout

| Dir | Contents | Stack |
|---|---|---|
| `firmware/` | Node firmware | ESP-IDF v6.0.2, target `esp32s3` |
| `server/` | Backend | Spring Boot 4.1, Java 25, Maven (built via Docker — no host JDK needed) |
| `webui/` | Web UI | Vue 3 + Vite + TS + Pinia + Nuxt UI v4 + ECharts |
| Root deployment files | `compose.yml`, `.env.example`, nginx and Mosquitto configuration | Docker Compose |
| `docs/design/` | Implementation-level design docs (authoritative detail) | |
| `docs/adr/` | Architecture decision records | |

Each part has its own README with build/run instructions. Commits follow Conventional Commits.

## Quick start (server stack)

```bash
cp .env.example .env         # then edit every password
docker compose up -d
```

Web UI: `http://localhost/` · API: `http://localhost/api/v1` · MQTT: `1883`

The `mosquitto-init` service creates or updates the server credential without deleting node users. The first server start creates the bootstrap administrator from `BOOTSTRAP_ADMIN_USERNAME` and `BOOTSTRAP_ADMIN_PASSWORD`; change that temporary password after login.

To use an external PostgreSQL instance, clear `COMPOSE_PROFILES` and set `DB_URL`, `DB_USER`, and `DB_PASSWORD` in `.env`. To add a node MQTT user:

```bash
docker compose exec mosquitto mosquitto_passwd -b /mosquitto/passwd/passwd node-esp32s3-xxxxxx <password>
docker compose kill -s SIGHUP mosquitto
```

## MQTT contract (invariant since Phase 1)

Base topic `home/{room}/{node_id}/…` — see the roadmap §1.2/§1.3 for the full table (status/discovery/relay/sensor/cmd). Nodes self-register via a retained `discovery` JSON payload; the server upserts nodes/capabilities automatically.

## Install ESP32-S3 firmware

If you download the artifact built on GitHub Actions:

1. Erase flash

```sh
esptool --chip esp32s3 --port /dev/cu.usbmodemXXXX erase-flash
```

2. Flash firmware

```sh
esptool --chip esp32s3 --port /dev/cu.usbmodemXXXX --baud 460800 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 4MB --flash-freq 80m 0x0 bootloader/bootloader.bin 0x8000 partition_table/partition-table.bin 0x10000 smart_home_node.bin
```
