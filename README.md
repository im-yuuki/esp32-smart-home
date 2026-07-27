# Smart Home ESP32-S3

Multi-room smart-home system: ESP32-S3 nodes (relays + sensors) connect over WiFi/MQTT to a central server with a realtime web UI. See `C:\Users\Izuki\Downloads\ROADMAP.md` (Vietnamese) for the full multi-phase roadmap; this repo currently implements **Phase 1 (Giai đoạn 1)** — one node, two relays, optional SHT31 sensor, no auth, LAN-only.

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
| `deploy/` | Server-stack deployment | Docker Compose (optional postgres:16, eclipse-mosquitto:2, server, webui/nginx) |
| `docs/design/` | Implementation-level design docs (authoritative detail) | |
| `docs/adr/` | Architecture decision records | |

Each part has its own README with build/run instructions. Commits follow Conventional Commits.

## Quick start (server stack)

```
cd deploy
copy .env.example .env       # then edit passwords
docker compose up -d
```

Web UI: `http://localhost/` · API: `http://localhost/api/v1` · MQTT: `1883`

## MQTT contract (invariant since Phase 1)

Base topic `home/{room}/{node_id}/…` — see the roadmap §1.2/§1.3 for the full table (status/discovery/relay/sensor/cmd). Nodes self-register via a retained `discovery` JSON payload; the server upserts nodes/capabilities automatically.
