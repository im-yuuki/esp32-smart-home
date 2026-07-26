# server/ — Spring Boot 4.1 backend (Phase 1)

The single server-side MQTT client of the system: subscribes `home/#`, persists
discovery/status/state/telemetry to Postgres (Flyway-managed schema), publishes relay
commands, and broadcasts normalized events to the web UI over STOMP/SockJS.

Stack: Spring Boot **4.1.0** (Jackson 3 / `tools.jackson`), Java 21, Spring Integration
MQTT 7.1.0 + Paho v3 (MQTT 3.1.1), Hibernate ORM 7.4, Flyway, PostgreSQL 16.

## Docker-only dev loop (no host Java)

Everything runs through the compose stack in `../deploy` (run all commands from there):

```bash
docker compose --profile tools run --rm mvn test        # unit tests (m2cache volume keeps repeat runs fast)
docker compose --profile tools run --rm mvn verify      # full check
docker compose up -d --build server                     # rebuild + restart backend
docker compose logs -f server                           # watch logs
```

Configuration is env-driven (`DB_URL`, `DB_USER`, `DB_PASSWORD`, `MQTT_URI`,
`MQTT_USERNAME`, `MQTT_PASSWORD`, `MQTT_CLIENT_ID`) — compose injects the values from
`deploy/.env`. Health: `http://localhost:8080/actuator/health`.

## REST surface (`/api/v1`, all responses in the `{"data":…,"error":…}` envelope)

| Method | Path | Notes |
|---|---|---|
| GET | `/nodes` | nodes + capabilities (capability `meta`/`lastState` are raw JSON) |
| GET | `/nodes/{nodeId}` | 404 envelope if unknown |
| POST | `/nodes/{nodeId}/relays/{ch}/command` | body `{"state":"ON"\|"OFF"}` → publish to `.../set`, **202 immediately** |
| GET | `/nodes/{nodeId}/sensors/latest` | `data: null` when no readings yet |
| GET | `/nodes/{nodeId}/sensors/history?from=&to=&bucket=` | ISO-8601 instants, default last 24 h, raw rows asc, capped 10 000; `bucket` accepted-and-ignored in Phase 1 |

WebSocket: STOMP endpoint `/ws` (SockJS), broadcast `/topic/events`, payload
`{type, nodeId, channel, data, ts}` with `ts` = epoch millis; broker heartbeats 10 s/10 s.

## Design notes / deviations

- **Two MQTT client IDs** — `server-core` (subscriber) + `server-core-pub` (publisher).
  The roadmap says "single client-id", but the inbound adapter and outbound handler each
  own a physical Paho connection and a broker kicks duplicate IDs into a reconnect loop.
- **Self-echo** — the server hears its own `.../set`/`.../cmd` publishes under `home/#`
  (MQTT v3 has no noLocal); the topic router ignores them (TRACE only).
- **Status for unknown node creates a stub row** — nodes publish retained `status` before
  `discovery` on boot, so drop-on-unknown would leave a new node offline until its next
  status publish. The stub (nodeId + room from the topic) is completed by discovery
  milliseconds later. Relay/sensor messages for unknown nodes/capabilities are still
  warn-and-dropped; retained replay self-heals them on the next (re)subscribe.
- **JSONB as `String`** + `@JdbcTypeCode(SqlTypes.JSON)`, re-emitted with `@JsonRawValue`:
  Hibernate 7.4's auto-detected JSON format mapper targets Jackson 2, which is not on the
  Boot 4 classpath. `ip` is a `String` bound as `inet` via `SqlTypes.INET`.
- **State-topic-is-truth** — POST command never waits for, nor fakes, the resulting
  state; `last_state` changes only when the node reports on `.../relay/{ch}/state`.
- Retained-replay ordering across topics is not MQTT-guaranteed (accepted Phase 1
  limitation; Mosquitto replays in storage order in practice).
