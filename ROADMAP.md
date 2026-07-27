# Lộ trình dự án Smart Home ESP32-S3

> Tài liệu đặc tả kỹ thuật dành cho agent lập trình. Mỗi giai đoạn có phạm vi, yêu cầu kỹ thuật chi tiết và tiêu chí nghiệm thu (acceptance criteria). Không chuyển sang giai đoạn sau khi giai đoạn trước chưa đạt toàn bộ tiêu chí nghiệm thu.

## 0. Tổng quan hệ thống

**Mục tiêu:** Hệ thống nhà thông minh gồm nhiều node ESP32-S3 đặt tại từng phòng (điều khiển relay bật/tắt đèn, đọc cảm biến nhiệt độ/độ ẩm, mở rộng module về sau), kết nối qua WiFi có sẵn về một máy chủ trung tâm. Máy chủ có web UI quản lí, đa tài khoản phân quyền RBAC theo phạm vi phòng/thiết bị, và về sau tích hợp Google Home / Apple HomeKit (ưu tiên qua Matter bridge) với danh sách thiết bị expose được lọc theo phân quyền.

**Stack đã chốt:**

| Thành phần | Công nghệ |
|---|---|
| Firmware node | ESP-IDF v5.x (C), target `esp32s3` |
| Giao thức thiết bị ↔ server | MQTT 3.1.1/5.0 qua TLS (từ GĐ2) |
| MQTT broker | Eclipse Mosquitto 2.x (Docker) |
| Backend | Spring Boot 3.x, Java 21 |
| Database | PostgreSQL 16 (+ TimescaleDB extension từ GĐ2) |
| Web UI | Vue 3 (Composition API) + Vite + TypeScript + Pinia + Vue Router |
| Realtime UI | WebSocket (STOMP over SockJS) từ backend |
| Triển khai server | Docker Compose |

**Kiến trúc luồng dữ liệu:**

```
[Vue SPA] ⇄ REST + WebSocket ⇄ [Spring Boot] ⇄ MQTT ⇄ [Mosquitto] ⇄ MQTT ⇄ [ESP32-S3 nodes]
                                      ⇅
                                [PostgreSQL]
```

- Node **không bao giờ** nói chuyện trực tiếp với web UI. Mọi lệnh đi qua backend để kiểm tra quyền và ghi audit log.
- Backend là MQTT client duy nhất phía server: subscribe toàn bộ `home/#`, publish lệnh điều khiển.
- Web UI nhận trạng thái realtime qua WebSocket do backend đẩy xuống sau khi nhận từ MQTT.

## 1. Quy ước chung (áp dụng cho mọi giai đoạn)

### 1.1. Định danh

- `node_id`: chuỗi `esp32s3-` + 6 hex cuối MAC, ví dụ `esp32s3-a4b2c1`. Sinh tự động trong firmware, không hard-code.
- `room`: slug ASCII không dấu, ví dụ `phong-khach`, `phong-ngu-1`. Node lưu trong NVS, cấu hình được.
- Mỗi module trên node có `channel` đánh số từ 1: `relay/1`, `relay/2`, `sensor/temperature`…

### 1.2. Hợp đồng MQTT (MQTT contract) — bất biến từ GĐ1

| Topic | Chiều | QoS | Retain | Payload |
|---|---|---|---|---|
| `home/{room}/{node_id}/status` | node → server | 1 | ✔ | `online` / `offline` (LWT đặt `offline`) |
| `home/{room}/{node_id}/discovery` | node → server | 1 | ✔ | JSON mô tả capability (xem 1.3) |
| `home/{room}/{node_id}/relay/{ch}/set` | server → node | 1 | ✘ | `{"state":"ON"}` hoặc `{"state":"OFF"}` |
| `home/{room}/{node_id}/relay/{ch}/state` | node → server | 1 | ✔ | `{"state":"ON","source":"mqtt\|button\|boot"}` |
| `home/{room}/{node_id}/sensor/state` | node → server | 0 | ✘ | `{"temperature":28.5,"humidity":65.2,"ts":1753500000}` |
| `home/{room}/{node_id}/cmd` | server → node | 1 | ✘ | Lệnh hệ thống: `{"action":"reboot"}`, `{"action":"ota","url":"..."}` (GĐ2) |

Quy tắc: node chỉ publish `state` **sau khi** đã thực thi xong lệnh (command → act → report). Server coi `state` là nguồn sự thật, không tự suy diễn trạng thái từ lệnh đã gửi.

### 1.3. Discovery payload

Node publish khi boot và khi cấu hình thay đổi. Server dùng message này để tự đăng ký node + capability vào DB — **đây là cơ chế mở rộng module**: thêm module mới chỉ cần firmware khai báo thêm capability, server không cần sửa code.

```json
{
  "node_id": "esp32s3-a4b2c1",
  "room": "phong-khach",
  "fw_version": "1.0.0",
  "ip": "192.168.1.51",
  "capabilities": [
    { "type": "relay",  "channel": 1, "name": "Đèn trần" },
    { "type": "relay",  "channel": 2, "name": "Đèn bàn" },
    { "type": "sensor", "channel": 1, "kind": "temperature_humidity", "model": "SHT31", "interval_s": 30 }
  ]
}
```

### 1.4. Quy ước repo

Monorepo, cấu trúc:

```
smart-home/
├── firmware/            # ESP-IDF project
├── server/              # Spring Boot
├── webui/               # Vue 3
├── deploy/              # docker-compose.yml, mosquitto config, init SQL
└── docs/                # tài liệu, ADR
```

Mỗi phần có README riêng ghi cách build/chạy. Commit theo Conventional Commits. Firmware version theo semver, nhúng vào binary qua `CMakeLists.txt`.

---

## 2. Giai đoạn 1 — MVP: 1 node, điều khiển relay + đọc cảm biến qua web

**Phạm vi:** 1 node ESP32-S3 (2 relay + 1 cảm biến SHT31 hoặc DHT22), Mosquitto chưa TLS (chỉ username/password, mạng LAN), Spring Boot chưa có auth (GĐ2 mới thêm), Vue hiển thị dashboard bật/tắt relay và xem nhiệt độ/độ ẩm realtime.

### 2.1. Firmware (ESP-IDF)

**Môi trường:** ESP-IDF v5.2+ (`idf.py set-target esp32s3`). Cấu hình qua `sdkconfig.defaults` commit vào repo.

**Cấu trúc component:**

```
firmware/
├── main/
│   ├── main.c               # app_main: khởi tạo tuần tự các module
│   ├── app_config.c/.h      # đọc/ghi cấu hình từ NVS (wifi, mqtt, room, tên relay)
│   ├── wifi_manager.c/.h    # kết nối STA, tự reconnect với exponential backoff
│   ├── mqtt_client.c/.h     # wrapper quanh esp-mqtt, LWT, hàng đợi publish
│   ├── relay_driver.c/.h    # điều khiển GPIO relay, lưu/khôi phục trạng thái qua NVS
│   ├── sensor_task.c/.h     # đọc SHT31 qua I2C, publish định kỳ
│   ├── button_handler.c/.h  # nút vật lý, debounce, điều khiển cục bộ
│   └── discovery.c/.h       # dựng và publish discovery JSON
└── CMakeLists.txt
```

**Yêu cầu kỹ thuật:**

1. **WiFi:** chế độ STA, SSID/password đọc từ NVS. GĐ1 cho phép nạp cấu hình lần đầu bằng `menuconfig` hoặc file `nvs_partition`. Mất WiFi → reconnect backoff 1s→2s→4s… tối đa 60s, không reset chip.
2. **MQTT:** dùng component `esp-mqtt`. Kết nối với username/password từ NVS. LWT: topic `.../status`, payload `offline`, retain, QoS 1. Khi kết nối thành công: publish `online` (retain) → publish discovery → publish trạng thái hiện tại của mọi relay → subscribe `home/{room}/{node_id}/relay/+/set` và `home/{room}/{node_id}/cmd`.
3. **Relay:** GPIO cấu hình trong `app_config` (mặc định GPIO 4, 5). Active-high/low cấu hình được. Trạng thái lưu NVS mỗi lần đổi; sau khi mất điện, boot khôi phục trạng thái cũ (power-on behavior cấu hình được: `restore` | `off` | `on`). Sau khi bật/tắt xong mới publish `state` kèm `source`.
4. **Nút vật lý:** mỗi relay 1 nút (GPIO input pull-up, debounce 50ms bằng timer, không dùng delay trong ISR). Nhấn → toggle relay **ngay lập tức tại node** (không chờ server), rồi publish `state` với `source:"button"`. Đây là yêu cầu bắt buộc: đèn phải điều khiển được khi mất mạng.
5. **Cảm biến:** SHT31 qua I2C (driver tự viết hoặc component từ ESP Component Registry). Task riêng đọc mỗi 30s (cấu hình được), lọc giá trị bất thường (ngoài −20…80°C, 0…100%RH thì bỏ qua và log). Publish QoS 0.
6. **Cấu trúc task:** mỗi module 1 FreeRTOS task hoặc chạy trên event loop; giao tiếp giữa button/relay/mqtt qua queue hoặc `esp_event`, **không** gọi publish MQTT từ ISR.
7. **Logging:** dùng `ESP_LOGx` với tag riêng từng module. Mức mặc định INFO.
8. **Watchdog:** bật task watchdog cho các task chính.

### 2.2. Broker (Mosquitto)

- Docker image `eclipse-mosquitto:2`. Tắt anonymous, tạo password file với 2 user: `server` (backend) và `node-esp32s3-a4b2c1`.
- File cấu hình + password file nằm trong `deploy/mosquitto/`.
- GĐ1 chưa cần ACL (thêm ở GĐ3) và chưa TLS (thêm ở GĐ2).

### 2.3. Backend (Spring Boot)

**Khởi tạo:** Spring Boot 3.3+, Java 21, Maven. Dependencies: `spring-boot-starter-web`, `spring-boot-starter-data-jpa`, `spring-boot-starter-websocket`, `spring-boot-starter-validation`, `spring-integration-mqtt` (dùng Eclipse Paho bên dưới), `postgresql`, `lombok`, `springdoc-openapi-starter-webmvc-ui`.

**Cấu trúc package `com.smarthome.server`:**

```
├── config/          # MqttConfig, WebSocketConfig, JacksonConfig
├── mqtt/            # MqttInboundHandler (route theo topic), MqttGateway (publish)
├── device/          # entity Node, Capability; NodeService, DeviceController
├── telemetry/       # entity SensorReading; TelemetryService, TelemetryController
├── realtime/        # StatePublisher: đẩy sự kiện qua STOMP
└── common/          # DTO, exception handler, ApiResponse wrapper
```

**Xử lý MQTT inbound:**

- Subscribe `home/#` với 1 client-id cố định `server-core`.
- Router theo pattern topic:
  - `.../discovery` → upsert `nodes` + `capabilities` (node mới tự xuất hiện trong hệ thống, không cần thao tác tay).
  - `.../status` → cập nhật `nodes.online`, `nodes.last_seen`.
  - `.../relay/{ch}/state` → cập nhật `capabilities.last_state` (JSONB), forward qua WebSocket.
  - `.../sensor/state` → insert `sensor_readings`, forward qua WebSocket.

**REST API (prefix `/api/v1`):**

| Method | Path | Mô tả |
|---|---|---|
| GET | `/nodes` | Danh sách node + capability + trạng thái online |
| GET | `/nodes/{nodeId}` | Chi tiết 1 node |
| POST | `/nodes/{nodeId}/relays/{ch}/command` | Body `{"state":"ON"}` → publish MQTT `.../set`, trả 202 |
| GET | `/nodes/{nodeId}/sensors/latest` | Giá trị cảm biến mới nhất |
| GET | `/nodes/{nodeId}/sensors/history?from=&to=&bucket=` | Lịch sử (GĐ1 trả raw, GĐ2 mới aggregate) |

Quy tắc: `POST command` chỉ publish lệnh và trả 202 Accepted — **không** chờ node phản hồi. UI cập nhật trạng thái khi nhận event `state` qua WebSocket. Nếu sau 5s không nhận được state, UI hiển thị cảnh báo "thiết bị không phản hồi".

**WebSocket:** STOMP endpoint `/ws`, topic broadcast `/topic/events`. Payload chuẩn hóa:

```json
{ "type": "RELAY_STATE" | "SENSOR_STATE" | "NODE_STATUS", "nodeId": "...", "channel": 1, "data": { ... }, "ts": 1753500000 }
```

**Schema DB GĐ1 (Flyway migration `V1__init.sql`):**

```sql
CREATE TABLE nodes (
  id          BIGSERIAL PRIMARY KEY,
  node_id     TEXT UNIQUE NOT NULL,
  room        TEXT NOT NULL,
  fw_version  TEXT,
  ip          INET,
  online      BOOLEAN NOT NULL DEFAULT FALSE,
  last_seen   TIMESTAMPTZ,
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE capabilities (
  id          BIGSERIAL PRIMARY KEY,
  node_pk     BIGINT NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  type        TEXT NOT NULL,            -- relay | sensor
  channel     INT  NOT NULL,
  name        TEXT,
  meta        JSONB NOT NULL DEFAULT '{}',
  last_state  JSONB,
  UNIQUE (node_pk, type, channel)
);

CREATE TABLE sensor_readings (
  id          BIGSERIAL PRIMARY KEY,
  node_pk     BIGINT NOT NULL REFERENCES nodes(id),
  temperature DOUBLE PRECISION,
  humidity    DOUBLE PRECISION,
  ts          TIMESTAMPTZ NOT NULL
);
CREATE INDEX idx_readings_node_ts ON sensor_readings (node_pk, ts DESC);
```

### 2.4. Web UI (Vue 3)

**Khởi tạo:** Vite + Vue 3 + TypeScript. Thư viện: `pinia`, `vue-router`, `axios`, `@stomp/stompjs`, `sockjs-client`, chart dùng `echarts` hoặc `chart.js`. UI framework tùy chọn (gợi ý: PrimeVue hoặc Naive UI) — chọn 1 và dùng nhất quán.

**Cấu trúc:**

```
webui/src/
├── api/           # axios instance + module theo resource (nodes.ts, telemetry.ts)
├── stores/        # Pinia: useNodesStore, useRealtimeStore
├── composables/   # useWebSocket (kết nối STOMP, auto-reconnect)
├── views/
│   ├── DashboardView.vue   # lưới card theo phòng
│   └── NodeDetailView.vue  # chi tiết node + biểu đồ cảm biến
└── components/
    ├── RelaySwitch.vue     # toggle + trạng thái pending/timeout
    ├── SensorCard.vue      # nhiệt độ, độ ẩm, thời điểm cập nhật
    └── NodeStatusBadge.vue # online/offline
```

**Yêu cầu hành vi:**

1. Dashboard nhóm thiết bị theo `room`. Mỗi relay là 1 toggle; bấm → gọi API → toggle vào trạng thái *pending* (spinner) → chỉ chuyển trạng thái thật khi nhận event WebSocket → quá 5s thì revert + toast lỗi.
2. WebSocket tự reconnect (backoff), có indicator kết nối trên header.
3. Node offline → card mờ đi, toggle disable.
4. Trang chi tiết node có biểu đồ nhiệt độ/độ ẩm 24h gần nhất (gọi API history).
5. Responsive: dùng tốt trên điện thoại (đây là thiết bị dùng chính trong nhà).

### 2.5. Deploy GĐ1

`deploy/docker-compose.yml` gồm: `postgres:16`, `eclipse-mosquitto:2`, `server` (build từ Dockerfile multi-stage của Spring Boot), `webui` (build Vite → nginx). Biến môi trường qua file `.env`. Volume cho Postgres và Mosquitto.

### 2.6. Tiêu chí nghiệm thu GĐ1

- [ ] Nạp firmware, node tự kết nối WiFi + MQTT, xuất hiện trong web UI trong vòng 10s mà không cần thao tác gì trên server (discovery hoạt động).
- [ ] Bật/tắt 2 relay từ web UI, trạng thái phản hồi trên UI < 1s trong LAN.
- [ ] Nhấn nút vật lý: relay đổi trạng thái ngay cả khi rút mạng WiFi; khi có mạng lại, trạng thái trên UI tự đồng bộ đúng.
- [ ] Rút điện node → UI hiển thị offline trong ≤ 90s (LWT + keepalive). Cắm lại → node khôi phục trạng thái relay như trước khi mất điện.
- [ ] Biểu đồ nhiệt độ/độ ẩm hiển thị dữ liệu 24h.
- [ ] `docker compose up -d` dựng được toàn bộ server từ máy sạch.

---

## 3. Giai đoạn 2 — Xác thực, RBAC, lịch sử telemetry, TLS, OTA

**Phạm vi:** hệ thống trở nên đa người dùng và an toàn: đăng nhập JWT, phân quyền RBAC theo phạm vi, audit log, MQTT qua TLS, lưu telemetry dài hạn với TimescaleDB, cập nhật firmware OTA từ server.

### 3.1. Xác thực & RBAC (Spring Security)

**Dependencies thêm:** `spring-boot-starter-security`, `jjwt` (hoặc `spring-boot-starter-oauth2-resource-server` với JWT tự cấp).

**Luồng auth:** `POST /api/v1/auth/login` (username + password, băm BCrypt) → trả `accessToken` (JWT, TTL 15 phút) + `refreshToken` (TTL 14 ngày, lưu DB, xoay vòng khi dùng). `POST /auth/refresh`, `POST /auth/logout` (revoke refresh token).

**Mô hình RBAC theo phạm vi (scope-based):**

```sql
CREATE TABLE users (
  id            BIGSERIAL PRIMARY KEY,
  username      TEXT UNIQUE NOT NULL,
  password_hash TEXT NOT NULL,
  display_name  TEXT,
  enabled       BOOLEAN NOT NULL DEFAULT TRUE,
  created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE roles (
  id    BIGSERIAL PRIMARY KEY,
  name  TEXT UNIQUE NOT NULL,        -- ADMIN, MEMBER, GUEST, CHILD...
  description TEXT
);

CREATE TABLE user_roles (
  user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,
  role_id BIGINT REFERENCES roles(id) ON DELETE CASCADE,
  PRIMARY KEY (user_id, role_id)
);

-- Quyền = action + scope. Scope quyết định "trên cái gì".
CREATE TABLE role_permissions (
  id         BIGSERIAL PRIMARY KEY,
  role_id    BIGINT NOT NULL REFERENCES roles(id) ON DELETE CASCADE,
  action     TEXT NOT NULL,   -- device.read | device.control | node.manage
                              -- user.manage | automation.manage | system.admin
  scope_type TEXT NOT NULL,   -- home | room | node | capability
  scope_ref  TEXT             -- NULL với home; slug phòng; node_id; node_id:type:channel
);

CREATE TABLE audit_logs (
  id         BIGSERIAL PRIMARY KEY,
  user_id    BIGINT REFERENCES users(id),
  action     TEXT NOT NULL,           -- vd: RELAY_COMMAND, LOGIN, USER_CREATE
  target     TEXT,                    -- vd: esp32s3-a4b2c1:relay:1
  detail     JSONB,
  ip         INET,
  ts         TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

**Quy tắc kiểm tra quyền (bắt buộc implement đúng):**

1. Quyền được cộng dồn từ mọi role của user.
2. Scope có tính bao trùm: `home` ⊃ `room` ⊃ `node` ⊃ `capability`. User được `device.control` scope `room:phong-khach` thì điều khiển được mọi relay của mọi node thuộc phòng đó.
3. Kiểm tra tại tầng service (không chỉ tầng controller) qua 1 component duy nhất `PermissionEvaluator.check(user, action, resource)` — mọi đường đi (REST, automation, integration GĐ4) đều phải qua hàm này.
4. Mọi lệnh điều khiển ghi 1 dòng `audit_logs` **trước khi** publish MQTT.
5. API quản trị: CRUD users, roles, gán permission — chỉ role có `user.manage`. Seed sẵn user `admin` + role `ADMIN` (full quyền) trong migration.

**Web UI thêm:** trang login, lưu token trong memory + refresh silent, route guard theo quyền (menu Quản trị chỉ hiện với `user.manage`), trang quản lí user/role với UI gán quyền theo phòng (checkbox matrix: role × phòng × action). UI ẩn/disable những thiết bị user không có `device.read`.

### 3.2. Telemetry dài hạn (TimescaleDB)

- Đổi image Postgres sang `timescale/timescaledb:latest-pg16`. Migration: `SELECT create_hypertable('sensor_readings','ts');`
- Continuous aggregate `sensor_readings_hourly` (avg/min/max theo giờ), retention policy: raw giữ 30 ngày, hourly giữ 2 năm.
- API `GET /sensors/history` thêm tham số `bucket=raw|hour|day`, backend chọn bảng phù hợp.

### 3.3. TLS cho MQTT

- Tạo CA nội bộ (script trong `deploy/certs/gen.sh` dùng openssl). Mosquitto listener 8883 với server cert.
- Backend kết nối `ssl://` và pin CA. Firmware nhúng CA cert qua `EMBED_TXTFILES`, dùng `esp-tls`; đồng bộ giờ bằng SNTP trước khi bắt tay TLS (bắt buộc, nếu không verify cert sẽ fail).
- Giữ listener 1883 chỉ bind localhost cho debug.

### 3.4. OTA firmware

**Firmware:** partition table 2 slot OTA (`ota_0`, `ota_1`) + `otadata`. Nhận lệnh `{"action":"ota","url":"https://server/api/v1/firmware/<file>","version":"1.1.0","sha256":"..."}` trên topic `cmd` → dùng `esp_https_ota` tải và verify SHA-256 → reboot vào slot mới → chạy ổn định 60s mới `esp_ota_mark_app_valid_cancel_rollback()`, ngược lại tự rollback. Bật `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`.

**Server:** API upload firmware (multipart, chỉ `system.admin`), lưu file + metadata (version, sha256, target). API/UI chọn node → gửi lệnh OTA, theo dõi tiến trình qua topic `home/.../ota/progress` node publish mỗi 10%.

### 3.5. Tiêu chí nghiệm thu GĐ2

- [ ] User không đăng nhập không gọi được bất kỳ API nào (trừ `/auth/login`), WebSocket cũng yêu cầu token.
- [ ] Tạo user role GUEST chỉ có `device.read` scope `room:phong-khach`: user này thấy đúng 1 phòng, gọi API điều khiển bị 403, UI không hiển thị toggle.
- [ ] Mọi lệnh relay có dòng audit log kèm user, thời điểm, thiết bị.
- [ ] Firmware kết nối broker qua TLS 8883; sniff mạng không đọc được payload.
- [ ] OTA từ UI thành công; giả lập firmware lỗi (panic trong 60s đầu) → node tự rollback về bản cũ.
- [ ] Truy vấn history 6 tháng với bucket=day trả về < 1s.

---

## 4. Giai đoạn 3 — Nhân rộng node & provisioning

**Phạm vi:** đưa hệ thống lên nhiều node (mỗi phòng 1 node) với quy trình thêm node mới hoàn toàn qua web, mỗi node một credential riêng, broker có ACL.

### 4.1. Provisioning node mới

**Luồng chuẩn (bắt buộc theo đúng thứ tự):**

1. Node mới (NVS trống) boot vào chế độ SoftAP: SSID `SmartHome-Setup-<4 hex>`, chạy HTTP server cấu hình tối giản (ESP-IDF component `wifi_provisioning` hoặc HTTP server tự viết, endpoint `POST /provision`).
2. Admin vào web UI → "Thêm node" → server sinh trước: bản ghi node ở trạng thái `PENDING`, cặp MQTT credential riêng (`node-<node_id>` / secret ngẫu nhiên 32 byte), và hiển thị JSON cấu hình (hoặc QR).
3. Admin kết nối vào SoftAP của node, gửi cấu hình: SSID/password WiFi, MQTT host/port, credential, room. Node lưu NVS, reboot sang STA.
4. Node kết nối, publish discovery → server đối chiếu `node_id`, chuyển trạng thái `ACTIVE`.
5. Nút BOOT giữ 5s = factory reset (xóa NVS, quay lại bước 1). Đây cũng là đường thu hồi thiết bị.

### 4.2. ACL Mosquitto

Chuyển Mosquitto sang dynamic security plugin (hoặc file ACL sinh tự động từ backend):

- User `node-<node_id>`: chỉ được publish/subscribe trong `home/+/<node_id>/#` của chính nó.
- User `server`: full `home/#`.
- Backend gọi API/ghi file ACL khi tạo/thu hồi node; thu hồi 1 node không ảnh hưởng node khác.

### 4.3. Quản lí vòng đời node

- UI: danh sách node theo phòng, đổi phòng/đổi tên (server gửi lệnh `{"action":"set_config",...}` qua topic `cmd`, node cập nhật NVS và re-publish discovery), vô hiệu hóa node (revoke MQTT credential), xem log OTA.
- OTA hàng loạt: chọn nhiều node cùng model, rollout tuần tự từng node (không song song), dừng nếu 1 node fail.

### 4.4. Tiêu chí nghiệm thu GĐ3

- [ ] Thêm 1 node mới từ hộp về đến hiện trên dashboard hoàn toàn qua web UI + điện thoại, không cần cắm USB (firmware nạp sẵn từ nhà máy/lần đầu).
- [ ] Node A dùng credential của mình không thể publish vào topic của node B (test bằng mosquitto_pub).
- [ ] Thu hồi node: credential bị vô hiệu ngay, node không kết nối lại được.
- [ ] Đổi phòng cho node từ UI: topic mới có hiệu lực sau 1 lần node reconnect, dữ liệu cũ vẫn truy vấn được.
- [ ] Hệ thống chạy ổn định với ≥ 5 node thật trong ≥ 7 ngày (không memory leak firmware — theo dõi `esp_get_free_heap_size` publish kèm heartbeat).

---

## 5. Giai đoạn 4 — Tích hợp Google Home & Apple Home (Matter bridge)

**Phạm vi:** expose thiết bị sang Google Home và Apple Home qua một **Matter bridge** chạy cạnh server, hoạt động trong LAN, danh sách thiết bị expose lọc theo phân quyền.

### 5.1. Kiến trúc

- Sidecar service `matter-bridge` (Node.js + thư viện `matter.js`, hoặc `python-matter-server`) chạy trong cùng Docker network, **network_mode: host** (Matter cần mDNS + IPv6 link-local).
- Bridge KHÔNG nói chuyện MQTT trực tiếp. Bridge gọi backend qua REST nội bộ + nhận event qua WebSocket nội bộ, dùng một **service account** có quyền do admin cấu hình. Nhờ đó `PermissionEvaluator` vẫn là chốt chặn duy nhất.
- Mapping: relay → Matter `OnOff Light` (hoặc `OnOff Plug-in Unit` tùy loại tải, cấu hình per-capability), cảm biến → `Temperature Sensor` + `Humidity Sensor`.

### 5.2. Expose theo phân quyền

- Backend thêm khái niệm **Expose Profile**: tập thiết bị + service account tương ứng. Mỗi profile = 1 bridge instance (port + QR pairing riêng).
- Ví dụ: profile "Cả nhà" expose toàn bộ, pair với home chính; profile "Phòng trẻ" chỉ expose 2 relay, pair riêng vào thiết bị của trẻ.
- Admin quản lí profile trong UI: chọn thiết bị, sinh QR code pairing, xem trạng thái commissioned.
- Ghi rõ trong tài liệu người dùng: sau khi pair, việc chia sẻ trong nội bộ Google/Apple Home do hệ sinh thái đó quản lí — RBAC của hệ thống kiểm soát ở ranh giới "thiết bị nào được đưa sang".

### 5.3. Yêu cầu kỹ thuật

1. Lệnh từ Google/Apple → bridge → `POST /nodes/.../command` với token service account → audit log ghi `user = svc:matter-<profile>`.
2. Trạng thái đổi từ bất kỳ nguồn nào (nút vật lý, web) phải phản ánh sang Google/Apple ≤ 2s (bridge subscribe WebSocket event và cập nhật attribute Matter).
3. Bridge restart không mất commissioning (persist fabric storage vào volume).
4. Nếu Matter không khả thi với thiết bị người dùng, phương án dự phòng ghi trong ADR: Google qua Smart Home Action cloud-to-cloud (cần OAuth2 server + domain public + HTTPS) và Apple qua HAP bridge (HomeKit Accessory Protocol) — giữ nguyên nguyên tắc mọi lệnh đi qua backend.

### 5.4. Tiêu chí nghiệm thu GĐ4

- [ ] Pair bridge với cả Google Home lẫn Apple Home bằng QR, bật/tắt đèn bằng giọng nói hoạt động.
- [ ] Bật đèn bằng nút vật lý → trạng thái trong app Google/Apple cập nhật ≤ 2s.
- [ ] Profile "Phòng trẻ" chỉ thấy đúng thiết bị được cấp; thiết bị khác không xuất hiện trong app.
- [ ] Rút Internet (giữ LAN): điều khiển qua Apple Home/Google Home local vẫn hoạt động.
- [ ] Mọi lệnh từ trợ lí ảo xuất hiện trong audit log với service account tương ứng.

---

## 6. Giai đoạn 5 — Automation engine & mở rộng module

**Phạm vi:** rule tự động hóa cấu hình từ UI và khung mở rộng loại module mới.

### 6.1. Automation engine (trong Spring Boot)

- Mô hình rule: **trigger** (sensor threshold, thay đổi trạng thái thiết bị, lịch cron, sunrise/sunset) + **condition** (khoảng thời gian, trạng thái thiết bị khác) + **action** (điều khiển thiết bị, gửi thông báo, delay).
- Lưu rule dạng JSONB trong bảng `automations`, đánh giá bằng engine tự viết (không cần rule engine ngoài): listener trên luồng event MQTT nội bộ + scheduler Spring cho trigger thời gian.
- Rule thuộc về user tạo ra nó và **chạy với quyền của user đó** (qua `PermissionEvaluator`) — user mất quyền thì rule tự vô hiệu.
- UI builder dạng form (chưa cần kéo thả), có nút "chạy thử", lịch sử thực thi 100 lần gần nhất.
- Chống lặp vô hạn: action do automation sinh ra được đánh dấu nguồn, một chuỗi trigger dây chuyền tối đa 5 cấp.

### 6.2. Khung mở rộng module

Thêm loại module mới (ví dụ: rèm cửa, PIR, đo công suất) chỉ gồm 3 việc, không đụng core:

1. **Firmware:** viết driver + khai báo capability mới trong discovery (`{"type":"cover","channel":1,...}` với `features` mô tả lệnh hỗ trợ).
2. **Backend:** đăng ký `CapabilityHandler` mới (interface: validate command, map topic, chuẩn hóa state) — cơ chế plugin qua Spring bean, tự phát hiện.
3. **Web UI:** đăng ký component hiển thị theo `type` trong registry (`capabilityComponents['cover'] = CoverCard`), fallback về card JSON generic nếu type chưa có UI.

Tài liệu `docs/ADDING_A_MODULE.md` mô tả checklist này kèm ví dụ hoàn chỉnh (module PIR).

### 6.3. Tiêu chí nghiệm thu GĐ5

- [ ] Rule "nhiệt độ phòng ngủ > 30°C trong 5 phút → bật quạt (relay), 22h–6h không kích hoạt" tạo từ UI và chạy đúng.
- [ ] User bị thu hồi quyền phòng X → rule của user đó trên phòng X ngừng thực thi và UI báo trạng thái.
- [ ] Thêm module PIR theo checklist: hiện trên UI và dùng được làm trigger automation mà không sửa file core nào.

---

## 7. Phi chức năng & bảo mật xuyên suốt

- **Mạng:** khuyến nghị tách node IoT vào VLAN/SSID riêng, firewall chỉ cho phép node ↔ server (MQTT 8883, HTTPS OTA) và NTP. Ghi hướng dẫn trong `docs/NETWORK.md`.
- **Server:** chạy trên mini PC/Raspberry Pi 5 + UPS. Backup: `pg_dump` hàng đêm + volume Mosquitto/Matter, script trong `deploy/backup/`.
- **Bí mật:** không commit secret; dùng `.env` (gitignore) + `.env.example`.
- **Chất lượng code:** backend có unit test cho `PermissionEvaluator` và MQTT router (JUnit + Testcontainers cho Postgres/Mosquitto); firmware có test host-based cho logic parse/format JSON (Unity/CMock); CI chạy build cả 3 phần.
- **Tài liệu:** mỗi quyết định kiến trúc lớn ghi 1 ADR trong `docs/adr/`.

## 8. Tóm tắt thứ tự thực hiện

| GĐ | Kết quả then chốt | Phụ thuộc |
|---|---|---|
| 1 | 1 node ESP-IDF + Spring Boot + Vue: điều khiển relay, xem cảm biến realtime | — |
| 2 | JWT + RBAC scope + audit log, TLS MQTT, TimescaleDB, OTA rollback | GĐ1 |
| 3 | Provisioning qua web, credential + ACL per-node, ≥5 node ổn định | GĐ2 |
| 4 | Matter bridge, expose theo Expose Profile, voice control | GĐ2, GĐ3 |
| 5 | Automation engine chạy theo quyền user, khung thêm module mới | GĐ2 |
