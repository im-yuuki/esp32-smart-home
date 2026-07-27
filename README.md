# Smart Home ESP32-S3

Multi-room smart-home system: ESP32-S3 nodes (relays + sensors) connect over WiFi/MQTT to a central server with a realtime web UI. The management server provides accounts, node approval, multi-group RBAC, and per-user realtime events without sending user or permission data to ESP32 nodes.

```
[Vue SPA] ⇄ REST + WebSocket ⇄ [Spring Boot] ⇄ MQTT ⇄ [Mosquitto] ⇄ MQTT ⇄ [ESP32-S3 nodes]
                                      ⇅
                                [PostgreSQL]
```

Nodes never talk to the web UI directly. The backend is the only server-side MQTT client; state topics are the single source of truth (the server never infers state from commands it sent).

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
