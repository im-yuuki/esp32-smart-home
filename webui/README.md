# webui — Smart Home SPA (Phase 1)

Vue 3 (Composition API, `<script setup lang="ts">`) + Vite + TypeScript + Pinia +
vue-router + vue-i18n + **shadcn-vue** source-owned components (Tailwind v4 + Reka UI) + ECharts (modular).
REST via axios, cookie-session authentication, group-scoped RBAC, and realtime via
STOMP/SockJS on `/ws` (subscribes `/user/queue/events`). All URLs are relative (`/api/...`, `/ws`) so the Vite dev
proxy and the prod same-origin nginx both work unchanged.

## Scripts

| Command | What it does |
|---|---|
| `npm run dev` | Dev server on :5173, proxies `/api` + `/ws` to `http://localhost:8080` |
| `npm run dev:mock` | Dev server in **mock mode** (`VITE_MOCK=1` via `.env.mock`) — no backend needed |
| `npm run build` | `vite build` then `vue-tsc` type-check (sequential on purpose, see below) |
| `npm run type-check` | `vue-tsc --build` alone |
| `npm run preview` | Serve the built `dist/` |

UI primitives live under `src/components/ui/` and are owned by this repository.
Use `components.json` when adding shadcn-vue components and keep generated code
aligned with the monochrome tokens in `src/assets/main.css`.

## Mock mode (`VITE_MOCK=1`)

Fully client-side fixture backend: 2 rooms, 3 nodes —

- `esp32s3-aabbcc` (Phong Khach): 2 relays + SHT31 sensor, acks relay commands
  after ~400 ms, sensor random-walk tick every 5 s.
- `esp32s3-c0ffee` (Phong Ngu): starts offline, flips online/offline every 45 s
  (exercises card dimming and pending-abort).
- `esp32s3-badbad` (Phong Ngu): **never acks** — deterministically exercises the
  5 s pending-timeout path (spinner → revert → "Device not responding" toast).

The mock socket feeds the exact same `applyEvent` store path as the real STOMP
subscription, so stores/components behave identically in both modes.

## Architecture notes

- **Pending/timeout state machine lives in `src/stores/nodes.ts`** (not in
  components): toggle → POST (202) → `pending` flag + 5 s timer → resolved only
  by the `RELAY_STATE` event (state topic is the source of truth — the event
  state wins even if it differs from the requested one) | timeout → revert +
  error toast | `NODE_STATUS offline` → abort early. Switches never flip
  optimistically and are disabled while pending, while the node is offline, and
  while the WS is down.
- **`src/composables/useWebSocket.ts`**: stompjs `Client` with
  `webSocketFactory: () => new SockJS(origin + '/ws')`, built-in exponential
  backoff 1 s → 30 s, 10 s/10 s heartbeats; every (re)connect refetches
  `/api/v1/nodes` (resync) and resubscribes `/user/queue/events`.
- **Timestamps**: everything past the api/event boundary is epoch **ms**.
  REST ISO-8601 strings and MQTT epoch-seconds payloads are normalized in
  `src/api/*` / `src/utils/time.ts`; components never convert.
- **Localization**: English and Vietnamese catalogs live in `src/i18n/locales/`.
  The initial language follows the browser, the header switcher persists the
  choice under `smarthome.locale`; date, relative-time and number formatters use
  the same locale.
- **Facility explorer**: folder icons are separate from map templates. The tree opens
  by default and persists collapsed branches per account in local storage. Users with
  `AUDIT_VIEW` access audit history from `/logs` and the compact activity rail.
- **Chart**: direct `echarts/core` modular imports via `useECharts`
  (init/ResizeObserver/dispose), time axis, dual y (°C / %), `lttb` sampling,
  `dataZoom: inside` for pinch zoom, live-appends `SENSOR_STATE` events.

## Production build / Docker

```sh
docker build -t smarthome-webui .
```

`node:26-alpine` build stage → `nginx:alpine` serving `dist/`. The image ships
a minimal standalone `nginx.conf` (SPA only, no proxy). In the root `compose.yml`
that file is bind-mounted over by the root `nginx.conf`, which adds the `/api` and `/ws` reverse proxies to
the `server` container — that file is the single deployment source of truth.

## Dev against the real stack

```sh
cd .. && docker compose up -d               # mysql + mosquitto + server
cd webui && npm run dev                     # proxies to localhost:8080
```
