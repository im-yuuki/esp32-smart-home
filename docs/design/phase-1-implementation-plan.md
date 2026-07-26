# Plan: Smart Home ESP32-S3 — Phase 1 (GĐ1) MVP

## Context

The user has a Vietnamese-language roadmap (`C:\Users\Izuki\Downloads\ROADMAP.md`) for a multi-phase ESP32-S3 smart-home system. This plan implements **Giai đoạn 1 (Phase 1)**: one ESP32-S3 node (2 relays + optional temp/humidity sensor) talking MQTT to Mosquitto; a Spring Boot backend that is the **sole** server-side MQTT client (subscribes `home/#`, publishes commands, treats state topics as source of truth); a Vue 3 web UI with realtime STOMP/SockJS updates; all deployable with `docker compose up -d`. The MQTT contract, discovery mechanism, REST surface, WS event payload, and `V1__init.sql` schema in the roadmap are **invariants — implement verbatim**.

### Decisions confirmed with the user
- **Scope**: full Phase 1 (firmware + broker + backend + web UI + deploy).
- **Sensor**: none on hand — sensor behind a driver interface (SHT31 first impl); firmware boots/runs/passes acceptance with sensor absent, omitting the sensor capability from discovery.
- **Backend**: Spring Boot **4.1.0** / Java 21 (user chose over roadmap's 3.x — 3.5 line hit OSS EOL 2026-06-30).
- **Web UI**: Vue 3 + Vite + TS + Pinia + vue-router + **Nuxt UI v4** (standalone via `@nuxt/ui/vite` plugin + `@nuxt/ui/vue-plugin`, Tailwind v4) + ECharts (direct, modular imports).
- **Layout**: roadmap monorepo at `C:\Users\Izuki\Projects\SmartHomeController` — `firmware/ server/ webui/ deploy/ docs/`; **remove the existing empty `esp32/`, `esp32-webui/`, `server-webui/` dirs** (user approved); `git init` at root, Conventional Commits.

### Environment facts (verified this session)
- **ESP-IDF v6.0.2** at `C:\Espressif\esp\v6.0.2\esp-idf` (roadmap assumed v5.x). Verified v6 deltas: `esp-mqtt` and `cJSON` are now **managed registry components** (`espressif/mqtt` 1.0.0, `espressif/cjson` 1.7.19), both present in the local offline mirror `C:\Espressif\tools\components\espressif\` (profile script presets `IDF_COMPONENT_LOCAL_STORAGE_URL=file://C:\Espressif\tools` → builds work offline); driver split (`esp_driver_gpio`/`esp_driver_i2c`, legacy `driver/i2c.h` EOL); warnings are errors (GCC 15); FreeRTOS headers need explicit includes.
- **No Java/Maven on host** → all backend build/test/run via Docker (29.6.1 present, Compose v5.3). Node 26 + npm 11 on host for webui dev. Git 2.55. COM1/COM4 present (board likely COM4).
- EIM toolchain activation: `. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1` (interactive) or `& "C:\Espressif\tools\python\v6.0.2\venv\Scripts\python.exe" "C:\Espressif\esp\v6.0.2\esp-idf\tools\idf.py" -C <dir> <args>` with `$env:IDF_COMPONENT_LOCAL_STORAGE_URL="file://C:\Espressif\tools"` (scripted).

## Full design documents (authoritative detail)

Produced by exploration/design agents this session. **Milestone 0 copies them into `docs/design/` so they survive the session.** Each is a complete file-by-file implementation spec; the sections below are executive summaries plus the cross-plan reconciliations.

| Doc | Location (JSON field in task output file) |
|---|---|
| ESP-IDF v6.0.2 ground-truth (3 reports) | `...\tasks\wu75vrgcc.output` → `result.migrationSystem`, `.wifiMqttJson`, `.periphStorageTools` |
| Firmware implementation plan | `...\tasks\wn74cwsdg.output` → `result.firmwarePlan` |
| Spring Boot 4 fact sheet; backend+deploy plan; webui plan | `...\tasks\w2ei2u6y3.output` → `result.boot4Facts`, `.backendPlan`, `.webuiPlan` |

(Full prefix: `C:\Users\Izuki\AppData\Local\Temp\claude\C--Users-Izuki-Projects-SmartHomeController\fdaf04e7-54ff-494d-8cdc-a496cf0e6d27\tasks\`)

## Milestones (implementation order)

- **M0 — Repo scaffold**: `git init`; remove empty `esp32/ esp32-webui/ server-webui/`; create `firmware/ server/ webui/ deploy/ docs/{design,adr}`; copy design docs; root README; 4 ADR stubs (IDF v6, Boot 4, Nuxt UI, sensor-optional); root `.gitignore`. Initial commit.
- **M1 — deploy/ skeleton**: postgres:16 + eclipse-mosquitto:2 in compose, `mosquitto.conf`, `.env`/`.env.example`, password-file bootstrap, fake-node scripts. Smoke: authed pub/sub round-trip; anonymous refused.
- **M2 — server/**: 8 increments (below), each smoke-tested against the fake node.
- **M3 — webui/**: mock-mode-first build (10 steps below), then integrate against the real stack; nginx same-origin prod wiring.
- **M4 — firmware/** (parallelizable with M2/M3 once M1 is up): 9 increments ending in acceptance hardening. Needs board on COM4.
- **M5 — End-to-end acceptance**: run the GĐ1 criteria checklist (bottom), `docker compose up -d` from clean state, per-part READMEs final.

---

## M2: Backend (`server/`) + M1 deploy — summary

**Stack (versions verified against Maven Central / Boot 4.1.0 BOM):** parent `spring-boot-starter-parent:4.1.0`, Java 21. Starters: **`spring-boot-starter-webmvc`** (Boot-4 rename — NOT `-web`), `-websocket`, `-validation`, `-data-jpa`, `-actuator`, `-integration` + `spring-integration-mqtt` (BOM 7.1.0) + **explicit `org.eclipse.paho:org.eclipse.paho.client.mqttv3:1.2.5`** (Paho is `<optional>` in SI 7). Flyway: `flyway-core` **and** `flyway-database-postgresql` (BOM 12.4.0). `postgresql` (42.7.11), Lombok 1.18.46. No security starter, no springdoc (3.0.3 not yet rebuilt for 4.1 — Phase 2), no Testcontainers (integration testing runs against the live compose stack).

**Package layout** per roadmap (`com.smarthome.server`: `config/ mqtt/ device/ telemetry/ realtime/ common/`) — full file tree in design doc.

**Key design points:**
- **MQTT wiring**: shared `DefaultMqttPahoClientFactory` (`setAutomaticReconnect(true)`, clean session true — safe since all interesting topics are retained). Inbound `MqttPahoMessageDrivenChannelAdapter(clientId, factory, "home/#")` QoS1 → `DirectChannel` (serial delivery = free ordering + no upsert races; adapter's `recoveryInterval` retry covers server-starts-before-broker). Outbound `MqttPahoMessageHandler` async, defaultQos 1. **Deviation (documented)**: two client IDs `server-core` (sub) + `server-core-pub` (pub) — one shared ID causes a broker kick-loop.
- **Topic routing**: `TopicParser.parse()` → sealed interface (`Status/Discovery/RelayState/SensorState/Ignored`); `Ignored` silently swallows the server's own `/set`+`/cmd` self-echo (v3 has no noLocal). `MqttInboundHandler` @ServiceActivator: try/catch-log whole body (exceptions must never kill Paho delivery); parse payload once into Jackson 3 `JsonNode`, feed both DB write and WS event.
- **Jackson 3 (Boot 4)**: runtime types `tools.jackson.*` (`JsonMapper`, `JsonNode`); customizer bean `JsonMapperBuilderCustomizer` (disable FAIL_ON_UNKNOWN_PROPERTIES); **annotations stay `com.fasterxml`** (`@JsonProperty`, `@JsonRawValue` valid). Never reference `MappingJackson2*` converters.
- **JSONB**: Hibernate 7.4's auto format-mapper targets Jackson 2 (absent from classpath) → map `meta`/`last_state` as **`String` + `@JdbcTypeCode(SqlTypes.JSON)`** (no mapper involved), re-emit via `@JsonRawValue` in DTO records. **INET**: `String` + `@JdbcTypeCode(SqlTypes.INET)`; fallback if it fights: plain String + `?stringtype=unspecified` on JDBC URL. `V1__init.sql` = roadmap SQL **verbatim**.
- **Discovery upsert** (`@Transactional`, serial by construction): upsert node (never touch `online` — status topic owns it); reconcile capabilities by `(type,channel)` preserving `last_state`, `orphanRemoval` for removed channels; `meta` = leftover JSON fields after removing type/channel/name (forward-compatible with future firmware fields).
- **REST `/api/v1`** per roadmap table; all responses in `ApiResponse<T>` envelope; `POST .../relays/{ch}/command` validates node+capability exist, publishes `{"state":...}` to `.../set`, returns **202 immediately** — never waits, never fakes state. History: raw rows, `from`/`to` ISO-8601 (defaults now−24h/now), asc, capped 10k rows; `bucket` accepted-and-ignored (Phase 1).
- **WebSocket**: STOMP `/ws` + SockJS, simple broker `/topic`, `EventMessage(type,nodeId,channel,data,ts)` with `ts` = epoch **millis**; `StatePublisher` → `/topic/events`. **Cross-plan fix: configure broker heartbeats** — `enableSimpleBroker("/topic").setHeartbeatValue(new long[]{10000,10000}).setTaskScheduler(<a TaskScheduler bean>)` — the UI relies on 10s/10s heartbeats for dead-connection detection.
- **Dockerfile**: `maven:3.9.11-eclipse-temurin-21` build (with `dependency:go-offline` cached layer) → `eclipse-temurin:21-jre-alpine`, non-root user; healthcheck uses busybox `wget` (no curl in alpine JRE).
- **deploy/**: compose services `postgres` (healthcheck-gated), `mosquitto` (1883 LAN, conf + passwd in named volume — avoids Windows bind-mount permission issues), `server`, `webui` (build context **`../webui`** — plans referenced `../server-webui`, superseded by the layout decision), and a `tools`-profile `mvn` service (shared `.m2` volume) giving `docker compose --profile tools run --rm mvn test` with no host Java. `mosquitto.conf`: explicit `listener 1883` + `allow_anonymous false` + `password_file`. One-time passwd bootstrap via `docker compose run --rm --entrypoint sh mosquitto -c "touch … && mosquitto_passwd -b …"` (broker exits if the file is missing); add real node users later with `mosquitto_passwd` + `SIGHUP` reload. `deploy/nginx/default.conf` bind-mounted into the webui container is the **single** nginx config (webui's own copy dropped — one source of truth): SPA `try_files` fallback, `/api/` and `/ws` proxied to `server:8080` with upgrade headers + `proxy_read_timeout 3600s`.
- **Fake-node tooling** (`deploy/scripts/`, bind-mounted RO into the mosquitto container — JSON never passes through PowerShell quoting): `fake-node-boot.sh` (retained status/discovery/relay states for `esp32s3-aabbcc` in `phong-khach`), `fake-node-sensor.sh`, `fake-node-relay.sh`, `watch-node-commands.sh`, plus `ws-watch.mjs` (host Node 26 + @stomp/stompjs) to print `/topic/events`.

**Backend implementation increments (each with smoke test in design doc):** ① deploy skeleton (=M1) → ② server skeleton + Flyway (health UP, tables created) → ③ entities/repos + read-only REST (validates JSONB/INET mapping at boot via `ddl-auto: validate`) → ④ MQTT inbound: discovery+status (fake-node-boot → GET /nodes shows node) → ⑤ relay/sensor state persistence → ⑥ WebSocket events (ws-watch sees SENSOR_STATE) → ⑦ outbound gateway + command endpoint (202; command visible on `/set`; lastState unchanged until fake node "acks") → ⑧ telemetry endpoints.

## M3: Web UI (`webui/`) — summary

**Structure** per roadmap (`api/ stores/ composables/ views/ components/` + `types/ utils/ api/mock/`) — full tree in design doc. Scaffold `npm create vue@latest` (TS/Router/Pinia), then Nuxt UI v4 standalone: `npm i @nuxt/ui tailwindcss @iconify-json/lucide`; `vite.config.ts` plugins `[vue(), ui()]`; `main.ts` `app.use(ui)` from `@nuxt/ui/vue-plugin`; CSS `@import "tailwindcss"; @import "@nuxt/ui";`; tsconfig includes for generated `auto-imports.d.ts`/`components.d.ts` (gitignored); root wrapped in **`<UApp>`** (required for `useToast`). Deps: axios, `@stomp/stompjs ^7.1` (built-in exponential backoff), sockjs-client (+ `define: { global: 'window' }` Vite shim), echarts ^6 modular.

**Key design points:**
- **Pending/timeout state machine lives in `useNodesStore`** (not components — the resolving RELAY_STATE event lands in the store; survives navigation; consistent across dashboard+detail): toggle → POST → `pending` flag + 5s timer (non-reactive module-scope map for timer ids) → resolve only on RELAY_STATE event (state-topic-is-truth: apply event state even if ≠ desired) | 5s timeout → revert + `toast.add({color:'error'})` | NODE_STATUS offline → abort early. Switch disabled while pending and while WS disconnected (a command that can't be confirmed would always timeout).
- **`useWebSocket`** (called once in App.vue): stompjs `Client` with `webSocketFactory: () => new SockJS(origin + '/ws')` (SockJS objects single-use), `reconnectTimeMode: EXPONENTIAL` 1s→30s cap, heartbeats 10s/10s; `onConnect` → **refetch `/nodes` (resync after missed events)** + subscribe `/topic/events` → `nodesStore.applyEvent`. Connection status → `useRealtimeStore` → header `ConnectionIndicator` (green Live / amber Reconnecting / red Offline). STOMP `Client` object never enters Pinia state.
- **Mock mode** (`VITE_MOCK=1`): fixture rooms/nodes incl. one offline and one never-acking node + `mockSocket` feeding the same `applyEvent` path — lets behaviors 1–3 (pending/timeout/dimming) be built and demoed before the backend exists, and deterministically exercises the 5s-timeout path.
- **Chart**: direct echarts via `useECharts` composable (init/ResizeObserver/dispose); time x-axis, dual y (°C/%), `sampling:'lttb'` (24h@30s ≈ 2880 pts), `dataZoom: inside` for pinch-zoom; history seeds a view-local buffer, live SENSOR_STATE events append.
- **Views/components** per roadmap: DashboardView (room-grouped responsive grid, mobile-first 1-col→3-col), NodeDetailView (meta + relays + chart, deep-link safe), NodeCard (dims + disables when offline), RelaySwitch (dumb: `USwitch` + `:loading="pending"`), SensorCard (relative "updated Xs ago"), NodeStatusBadge.
- **Cross-plan reconciliations (webui adapts to backend reality):** ① REST DTO = backend's `NodeDto{nodeId,room,fwVersion,ip,online,lastSeen,capabilities[]}` where capability `lastState`/`meta` are raw JSON — `api/nodes.ts` maps capabilities → `relays[]`/`sensor` view models (webui plan's assumed shape is the *view* model only). ② History endpoint takes `from`/`to` ISO-8601 (not `?hours=`) — omit params for the 24h default. ③ Timestamps: WS event `ts` = epoch ms; history rows `ts` = ISO-8601 Instant string; sensor MQTT payload `ts` = epoch s — **normalize everything to epoch ms in the api/event boundary layer**, components never convert.

**Webui implementation steps:** ① scaffold ② Nuxt UI standalone wiring (verify UButton + toast) ③ types + api layer + mock ④ stores + mock socket ⑤ **Milestone A: full dashboard on mock data** (all pending/timeout/offline behaviors demoable) ⑥ detail view + chart ⑦ real STOMP path ⑧ responsive pass at 375px + real phone via `--host` ⑨ integrate real backend (mock off, Vite proxy `/api`+`/ws` → localhost:8080) ⑩ prod build + Dockerfile (node:26-alpine → nginx:alpine) in compose.

## M4: Firmware (`firmware/`) — summary

**Module layout** per roadmap with one justified rename: `mqtt_client.c/.h` → **`mqtt_mgr.c/.h`** (managed component's public header is literally `mqtt_client.h`; a local file would shadow it under `INCLUDE_DIRS "."`). Modules: `main.c` (init order: NVS → app_config → event loop → **relay_driver → button_handler → sensor probe → wifi → mqtt** — relays restore before network so local control never waits on WiFi; sensor/button failures non-fatal), `app_config` (NVS ns `shc_cfg` config / `shc_state` relay states; seeded once from `Kconfig.projbuild` `CONFIG_SHC_*` when `cfg_ver` absent — NVS is source of truth thereafter; re-seed via `erase-flash`), `wifi_manager` (canonical v6 STA bring-up; esp_timer backoff 1s→2s→…60s cap, chip never resets; posts `APP_EVENT_WIFI_GOT_IP/LOST`; caches IP string; starts SNTP once — `esp_netif_sntp.h`, one isolated call with documented fallback), `mqtt_mgr` (LWT `status`="offline" retained QoS1, keepalive 30s → LWT fires ≤45s, inside the 90s budget; on CONNECTED: online → discovery → all relay states → subscribe `relay/+/set`+`cmd` via `esp_mqtt_client_subscribe_single`; esp-mqtt owns MQTT reconnect, `esp_mqtt_client_reconnect()` shortcut on GOT_IP; all publishes via `esp_mqtt_client_enqueue` outbox; `cmd` reboot = publish offline → 1.5s esp_timer → `esp_restart()`), `relay_driver` (**single relay_task prio 10 + `relay_cmd_q` is the only writer of relay GPIOs** — serializes MQTT/button/boot; strict act → NVS persist → report order; TWDT-fed), `button_handler` (ISR = `esp_timer_stop`+`start_once(50ms)` only; callback re-samples level, enqueues TOGGLE source=button — offline path end-to-end), `sensor_driver.h` vtable + `sht31.c` (i2c_master API: probe 0x44/0x45; read = transmit `{0x24,0x00}` → 16ms delay → receive 6B + CRC-8 — **not** `transmit_receive`, no room for conversion delay; NACK = `ESP_ERR_INVALID_RESPONSE` in v6), `sensor_task` (**absent sensor ⇒ warn, no task, capability omitted from discovery — the current acceptance configuration**; present: 30s period in 1s TWDT-fed slices, range filter −20…80°C/0…100%RH, QoS0 snprintf payload), `discovery` (cJSON; `fw_version` = `esp_app_get_description()->version`).

**Defaults (Kconfig-overridable, seeded to NVS):** relays GPIO 4/5 active-high, buttons GPIO 6/7 pull-up, I2C SDA 8/SCL 9, power-on `restore`, interval 30s; **set default room = `phong-khach`** (amend design doc's `livingroom` — matches roadmap examples + fake-node scripts). `node_id` = `esp32s3-`+last-6-hex STA MAC.

**Build files:** root CMakeLists: `set(PROJECT_VER "1.0.0")` before `project.cmake` (do NOT also set `CONFIG_APP_PROJECT_VER` — precedence trap). `main/idf_component.yml`: `espressif/mqtt ^1.0.0` + `espressif/cjson ^1.7.19` (offline mirror; commit `dependencies.lock`; gitignore `managed_components/`, `sdkconfig`, `build/`). `main/CMakeLists.txt` `PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_timer esp_event esp_wifi esp_netif nvs_flash esp_hw_support esp_app_format mqtt` (never `driver`/`json`). `partitions.csv`: nvs 24K/phy 4K/factory 3M + `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`. `sdkconfig.defaults`: `CONFIG_FREERTOS_HZ=1000`, TWDT panic + 10s, `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096`, log INFO, commented GCC-15 warning escape hatches (bring-up only, removed before sign-off).

**v6 pitfalls checklist** (full list in design doc §5): explicit FreeRTOS includes; MQTT `event->topic` not NUL-terminated + fragment guard; `esp_mqtt_client_config_t` nested fields (verified byte-for-byte from mirror zip); `WIFI_IF_STA` not `ESP_IF_WIFI_STA`; removed WiFi reason enums — log numerically; `vTaskDelayUntil` removed; `esp_ota_get_app_description` → `esp_app_get_description`; stack sizes in bytes; never call `esp_task_wdt_init` (auto-inited).

**Firmware increments (smoke tests in design doc §6):** ① scaffold + offline first build (managed components resolve; `idf.py size` fits) → ② app_config+main (NVS seed logs; `%.1f` float-format check) → ③ relay+button offline core (button toggles with no WiFi configured at all) → ④ wifi_manager (backoff observed 1/2/4…60s, no reboot) → ⑤ mqtt_mgr+discovery (connect sequence order on `mosquitto_sub -t 'home/#' -v`; retained replay) → ⑥ relay via MQTT + source attribution + malformed-payload robustness → ⑦ cmd reboot + LWT ≤45s → ⑧ sensor-absent verification (no capability, no traffic, fully functional) → ⑨ hardening (TWDT panic test; escape hatches removed; clean build).

## Verification — GĐ1 acceptance criteria mapping

Run after M5, from `deploy/`: `docker compose up -d` (clean machine path: only `.env` creation + passwd bootstrap precede it — both in README).

| Roadmap criterion | How verified |
|---|---|
| Node appears in web UI ≤10s after flash, zero server-side action | Board on COM4, broker up → watch dashboard; node auto-appears via discovery upsert (expect 4–7s). Server/UI half also provable without hardware via `fake-node-boot.sh`. |
| Toggle 2 relays from UI, response <1s on LAN | Click toggle → pending spinner → flips on RELAY_STATE event; verify round-trip feel + audible click. |
| Physical button works with WiFi unplugged; UI resyncs on reconnect | Kill AP/broker → press button → relay clicks immediately → restore network → retained `relay/{ch}/state` republished in connect sequence → UI shows correct state (also exercises UI resync-on-reconnect refetch). |
| Unplug node → UI offline ≤90s; replug → relay states restored | LWT keepalive 30s ⇒ offline ≤45s → NODE_STATUS event dims card; power-on `restore` re-applies NVS states before WiFi. |
| 24h temp/humidity chart | No sensor on hand: run `fake-node-sensor.sh` in loop mode (or backfill script) → chart renders 24h window + live appends. Real-sensor re-check deferred until an SHT31 arrives (wire SDA 8/SCL 9, reboot — capability auto-appears). |
| `docker compose up -d` builds whole server stack from clean machine | Fresh clone test: `.env` from example → passwd bootstrap → `up -d` → dashboard at `http://localhost/` (nginx same-origin SPA+/api+/ws). |

Cross-cutting behaviors also verified (design docs' runbooks): 202-then-event command flow with never-acking node (5s revert + toast), WS reconnect indicator with growing backoff gaps, offline-node toggle disable, mobile 375px pass + real phone via LAN, `vue-tsc` clean, unit tests `TopicParserTest`/`DiscoveryPayloadTest` green via `docker compose --profile tools run --rm mvn test`.

## Risks / notes

- **Spring Boot 4.1 + Jackson 3** is the newest surface here; the JSONB-as-String + `@JsonRawValue` design deliberately avoids the known Hibernate-format-mapper trap; INET has a documented fallback. Never copy Boot-3-era coordinates (`-starter-web`, `MappingJackson2*`).
- **First firmware build** resolves managed components from the local mirror — if the component manager unexpectedly goes to network and fails, the mirror URL env var is the fix (documented in firmware README).
- **Board flash size unknown** (4MB assumed — safe minimum for S3 modules); if `idf.py flash` reports 8/16MB, bump `CONFIG_ESPTOOLPY_FLASHSIZE`.
- **springdoc omitted** in Phase 1 (not yet rebuilt for Boot 4.1) — roadmap lists it; add in Phase 2 when it catches up. Documented in ADR-002.
- Retained-replay ordering across topics isn't MQTT-guaranteed; unknown-node states warn-and-drop and self-heal via retain semantics (accepted Phase 1 limitation, in README).
