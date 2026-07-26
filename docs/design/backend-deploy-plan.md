Docker 29.6.1 + Compose v5.3.0 and Node 26.5.0 confirmed; no Java/Maven, as stated. The repo is empty scaffolding: `esp32/`, `esp32-webui/`, `server/`, `server-webui/` (no `deploy/` yet â€” note the roadmap says `firmware/`/`webui/`; actual dirs are `esp32/`/`server-webui/`, so the compose webui build context must point at `../server-webui`). I have everything needed; here is the plan.

---

# Phase 1 Implementation Plan â€” `server/` (Spring Boot 4.1) + `deploy/` (docker-compose + Mosquitto)

Repo: `C:\Users\Izuki\Projects\SmartHomeController`. No Java/Maven on host â€” **every build, test, and run goes through Docker** (29.6.1, Compose v5.3.0). Node 26.5.0 is available on the host for the WebSocket watcher. `deploy/` does not exist yet and must be created. The Vue SPA lives in `server-webui/` (roadmap calls it `webui/`) â€” compose references `../server-webui`.

## 1. Complete file tree

```
server/
â”œâ”€ Dockerfile                      # multi-stage: maven:3.9.11-eclipse-temurin-21 -> eclipse-temurin:21-jre-alpine
â”œâ”€ .dockerignore                   # target/, .idea/, *.iml
â”œâ”€ pom.xml
â”œâ”€ README.md                       # docker-only build/test/run commands
â””â”€ src/
   â”œâ”€ main/java/com/smarthome/server/
   â”‚  â”œâ”€ SmartHomeServerApplication.java
   â”‚  â”œâ”€ config/
   â”‚  â”‚  â”œâ”€ MqttProperties.java          # @ConfigurationProperties("smarthome.mqtt") record
   â”‚  â”‚  â”œâ”€ MqttConfig.java              # client factory, inbound adapter, outbound handler, channels
   â”‚  â”‚  â”œâ”€ WebSocketConfig.java         # STOMP /ws + SockJS, /topic broker
   â”‚  â”‚  â”œâ”€ JacksonConfig.java           # JsonMapperBuilderCustomizer (Jackson 3)
   â”‚  â”‚  â””â”€ WebConfig.java               # CORS open for dev
   â”‚  â”œâ”€ mqtt/
   â”‚  â”‚  â”œâ”€ TopicParser.java             # topic string -> sealed ParsedTopic
   â”‚  â”‚  â”œâ”€ MqttInboundHandler.java      # @ServiceActivator router for home/#
   â”‚  â”‚  â”œâ”€ MqttGateway.java             # @MessagingGateway publish(topic, payload)
   â”‚  â”‚  â””â”€ DiscoveryPayload.java        # record mirror of discovery JSON
   â”‚  â”œâ”€ device/
   â”‚  â”‚  â”œâ”€ Node.java  Capability.java   # entities
   â”‚  â”‚  â”œâ”€ NodeRepository.java  CapabilityRepository.java
   â”‚  â”‚  â”œâ”€ NodeService.java             # discovery upsert, status, relay-state, command
   â”‚  â”‚  â””â”€ DeviceController.java        # GET /nodes, GET /nodes/{id}, POST .../relays/{ch}/command
   â”‚  â”œâ”€ telemetry/
   â”‚  â”‚  â”œâ”€ SensorReading.java  SensorReadingRepository.java
   â”‚  â”‚  â”œâ”€ TelemetryService.java
   â”‚  â”‚  â””â”€ TelemetryController.java     # sensors/latest, sensors/history
   â”‚  â”œâ”€ realtime/
   â”‚  â”‚  â”œâ”€ EventMessage.java            # record {type,nodeId,channel,data,ts}
   â”‚  â”‚  â””â”€ StatePublisher.java          # SimpMessagingTemplate -> /topic/events
   â”‚  â””â”€ common/
   â”‚     â”œâ”€ ApiResponse.java  NotFoundException.java  GlobalExceptionHandler.java
   â”‚     â””â”€ dto/ NodeDto.java  CapabilityDto.java  RelayCommandRequest.java  SensorReadingDto.java
   â”œâ”€ main/resources/
   â”‚  â”œâ”€ application.yml
   â”‚  â””â”€ db/migration/V1__init.sql       # exactly the roadmap SQL
   â””â”€ test/java/com/smarthome/server/
      â”œâ”€ mqtt/TopicParserTest.java
      â””â”€ mqtt/DiscoveryPayloadTest.java  # JSON -> record mapping w/ Jackson 3

deploy/
â”œâ”€ docker-compose.yml
â”œâ”€ .env.example
â”œâ”€ .gitignore                      # .env
â”œâ”€ README.md                       # bootstrap + verification runbook
â”œâ”€ mosquitto/
â”‚  â””â”€ mosquitto.conf
â”œâ”€ nginx/
â”‚  â””â”€ default.conf                 # SPA + /api + /ws reverse proxy
â””â”€ scripts/                        # bind-mounted RO into mosquitto container at /scripts
   â”œâ”€ fake-node-boot.sh            # retained status+discovery+relay states
   â”œâ”€ fake-node-sensor.sh          # sensor publish (single or loop)
   â”œâ”€ fake-node-relay.sh           # publish relay state (simulates node executing a command)
   â”œâ”€ watch-node-commands.sh       # mosquitto_sub on .../set and .../cmd
   â”œâ”€ ws-watch.mjs                 # STOMP watcher (host Node 26)
   â””â”€ package.json                 # dep: @stomp/stompjs
```

(`server-webui/Dockerfile` â€” node:26-alpine build â†’ nginx:alpine â€” belongs to the webui workstream; compose assumes it exists, sketch in Â§4.)

## 2. `server/pom.xml` dependencies (exact, per fact sheet)

Parent: `org.springframework.boot:spring-boot-starter-parent:4.1.0`, `<java.version>21</java.version>`.

| Dependency | Version | Note |
|---|---|---|
| `spring-boot-starter-webmvc` | BOM | **Boot 4 rename â€” NOT `spring-boot-starter-web`** (old name only a deprecated shim) |
| `spring-boot-starter-websocket` | BOM | unchanged name |
| `spring-boot-starter-validation` | BOM | for `RelayCommandRequest` |
| `spring-boot-starter-data-jpa` | BOM | Hibernate ORM 7.4.1.Final |
| `spring-boot-starter-actuator` | BOM | health for compose healthcheck |
| `spring-boot-starter-integration` | BOM | |
| `org.springframework.integration:spring-integration-mqtt` | BOM (7.1.0) | matches Boot 4.1.0 BOM exactly |
| `org.eclipse.paho:org.eclipse.paho.client.mqttv3` | **1.2.5 explicit** | Paho is `<optional>` in SI 7.1.0 â€” must be declared |
| `org.flywaydb:flyway-core` | BOM (12.4.0) | |
| `org.flywaydb:flyway-database-postgresql` | BOM | **required** â€” Flyway 10+ split PG support out of core |
| `org.postgresql:postgresql` | BOM (42.7.11) | scope runtime |
| `org.projectlombok:lombok` | BOM (1.18.46) | optional; entities only â€” DTOs are records |
| `spring-boot-starter-test` | BOM, test | |

Deliberately omitted in Phase 1: security starter (no auth), springdoc (3.0.3 works with Boot 4 but not yet rebuilt against 4.1 â€” add later if wanted), Testcontainers (integration testing is done against the live compose stack instead; avoids Docker-in-Docker complexity on Windows).

MQTT protocol choice: **spring-integration-mqtt with the Paho v3 (MQTT 3.1.1) adapter** `MqttPahoMessageDrivenChannelAdapter`/`MqttPahoMessageHandler`. Rationale: BOM-managed (zero version friction per fact sheet), free lifecycle/reconnect handling, and ESP-IDF's esp-mqtt speaks 3.1.1 by default so v5 buys nothing in Phase 1. Plain Paho would work but re-implements reconnect/lifecycle for no gain.

## 3. Backend component design

### 3.1 `application.yml` (env-var driven)

```yaml
server:
  port: 8080
spring:
  application.name: smarthome-server
  datasource:
    url: ${DB_URL:jdbc:postgresql://localhost:5432/smarthome}
    username: ${DB_USER:smarthome}
    password: ${DB_PASSWORD:smarthome}
  jpa:
    hibernate.ddl-auto: validate     # Flyway owns schema
    open-in-view: false
smarthome:
  mqtt:
    uri: ${MQTT_URI:tcp://localhost:1883}
    username: ${MQTT_USERNAME:server}
    password: ${MQTT_PASSWORD:changeme}
    client-id: ${MQTT_CLIENT_ID:server-core}
management:
  endpoints.web.exposure.include: health,info
```

`MqttProperties` = `@ConfigurationProperties(prefix="smarthome.mqtt")` record `(String uri, String username, String password, String clientId)`, enabled via `@ConfigurationPropertiesScan`.

### 3.2 MQTT wiring (`config/MqttConfig.java`)

One shared `DefaultMqttPahoClientFactory` with `MqttConnectOptions`: server URI, username/password, `setAutomaticReconnect(true)`, `setCleanSession(true)` (safe: all interesting topics are retained, so every (re)subscribe replays full state).

**Inbound** â€” `MqttPahoMessageDrivenChannelAdapter(clientId, factory, "home/#")`, QoS 1, output to a `DirectChannel` `mqttInboundChannel`. DirectChannel on purpose: Paho delivers messages serially on one thread, which gives us free ordering (discovery before state on node boot) and removes DB races in the upsert. The SI adapter also schedules its own reconnect attempts (`recoveryInterval`, default 10 s) when the broker isn't up yet at startup â€” important because compose may start the server before Mosquitto accepts connections.

**Outbound** â€” `MqttPahoMessageHandler(clientId + "-pub", factory)` as `@ServiceActivator(inputChannel="mqttOutboundChannel")`, `setDefaultQos(1)`, `setAsync(true)` (POST returns 202 without waiting, per spec).

> **Gotcha (spec deviation, unavoidable):** the roadmap says "single client-id server-core". SI's inbound adapter and outbound handler each own a physical Paho connection, and a broker disconnects duplicate client IDs (a reconnect kick-loop). Plan: `server-core` (inbound/subscriber) + `server-core-pub` (outbound/publisher). Document in README.

**Gateway** (`mqtt/MqttGateway.java`):

```java
@MessagingGateway(defaultRequestChannel = "mqttOutboundChannel")
public interface MqttGateway {
    void publish(@Header(MqttHeaders.TOPIC) String topic, String payload);
}
```
(Boot's integration auto-config scans `@MessagingGateway` interfaces in the base package â€” no extra `@IntegrationComponentScan` needed.)

### 3.3 Topic routing (`mqtt/TopicParser.java`, `MqttInboundHandler.java`)

```java
public sealed interface ParsedTopic {
    record Status(String room, String nodeId) implements ParsedTopic {}
    record Discovery(String room, String nodeId) implements ParsedTopic {}
    record RelayState(String room, String nodeId, int channel) implements ParsedTopic {}
    record SensorState(String room, String nodeId) implements ParsedTopic {}
    record Ignored(String topic) implements ParsedTopic {}

    static ParsedTopic parse(String topic) {
        String[] p = topic.split("/");
        if (p.length < 4 || !"home".equals(p[0])) return new Ignored(topic);
        return switch (p[3]) {
            case "status"    -> new Status(p[1], p[2]);
            case "discovery" -> new Discovery(p[1], p[2]);
            case "sensor"    when p.length == 5 && "state".equals(p[4]) -> new SensorState(p[1], p[2]);
            case "relay"     when p.length == 6 && "state".equals(p[5]) -> new RelayState(p[1], p[2], Integer.parseInt(p[4]));
            default          -> new Ignored(topic);   // catches /set, /cmd, malformed
        };
    }
}
```

> **Gotcha â€” self-echo:** the server subscribes `home/#` and publishes `.../relay/{ch}/set` and `.../cmd`, so MQTT echoes its **own commands back to it** (v3 has no noLocal). The `Ignored` arm silently drops them; log at TRACE only.

`MqttInboundHandler` is a `@ServiceActivator(inputChannel="mqttInboundChannel")` taking `Message<String>`, reading `MqttHeaders.RECEIVED_TOPIC`, switching on the sealed type, delegating to `NodeService`/`TelemetryService`, then calling `StatePublisher`. Wrap the whole body in try/catch-log â€” an exception thrown into the Paho callback must never kill message delivery.

Payload parsing uses the injected Jackson 3 `JsonMapper` (`tools.jackson.databind.json.JsonMapper`) into `JsonNode` once per message; the same node feeds the DB write (`node.toString()` for JSONB columns) and the WS event (`data` field).

Routing actions:
- **Status**: `nodeService.updateStatus(nodeId, "online".equals(payload))` â†’ set `online`, `last_seen = now()`. WS `NODE_STATUS` with `data = {"online":true|false}`. LWT "offline" flows through here automatically.
- **Discovery**: parse `DiscoveryPayload`, `nodeService.upsertFromDiscovery(...)` (Â§3.5). WS `NODE_STATUS` optional refresh event.
- **RelayState**: `nodeService.updateRelayState(nodeId, ch, rawJson)` â†’ set `capabilities.last_state`. WS `RELAY_STATE`, `channel = ch`, `data = {"state":...,"source":...}`.
- **SensorState**: `telemetryService.record(nodeId, temp, hum, ts)` â€” `ts` is **epoch seconds** â†’ `Instant.ofEpochSecond`, fallback `Instant.now()` if absent. WS `SENSOR_STATE`.
- Unknown `nodeId` for status/relay/sensor: WARN + drop. Retained-replay ordering across different topics under one `home/#` subscription is not guaranteed by MQTT, but Mosquitto in practice replays in storage order and the firmware contract publishes discovery before state on boot; a dropped retained relay-state self-heals on the next state change or server restart (retained msgs replay on every subscribe). Accepted Phase 1 limitation â€” document it.

### 3.4 Entities and the JSONB / INET decisions

**Key Boot 4 gotcha â€” Hibernate JSON format mapper vs Jackson 3.** Hibernate 7.4's auto-detected `JacksonJsonFormatMapper` targets **Jackson 2** (`com.fasterxml...ObjectMapper`), which is *not on the classpath* in a Boot 4 app. Mapping JSONB to `Map<String,Object>` therefore risks a missing-format-mapper failure. Sidestep the whole class of problems: **map JSONB columns as `String`** â€” with `@JdbcTypeCode(SqlTypes.JSON)` on a `String` attribute Hibernate passes the raw text through as jsonb with **no format mapper involved**. DTOs re-emit it verbatim with `@JsonRawValue` (jackson-annotations stays `com.fasterxml` 2.21 under Jackson 3 â€” still valid).

```java
@Entity @Table(name = "nodes")
public class Node {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY) private Long id;
    @Column(name = "node_id", nullable = false, unique = true) private String nodeId;
    @Column(nullable = false) private String room;
    @Column(name = "fw_version") private String fwVersion;
    @JdbcTypeCode(SqlTypes.INET) @Column(columnDefinition = "inet") private String ip;   // see note
    @Column(nullable = false) private boolean online;
    @Column(name = "last_seen") private Instant lastSeen;
    @Column(name = "created_at", nullable = false, insertable = false, updatable = false) private Instant createdAt;
    @OneToMany(mappedBy = "node", cascade = ALL, orphanRemoval = true) private List<Capability> capabilities = new ArrayList<>();
}

@Entity @Table(name = "capabilities",
    uniqueConstraints = @UniqueConstraint(columnNames = {"node_pk","type","channel"}))
public class Capability {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY) private Long id;
    @ManyToOne(fetch = LAZY) @JoinColumn(name = "node_pk", nullable = false) private Node node;
    @Column(nullable = false) private String type;      // "relay" | "sensor"
    @Column(nullable = false) private int channel;
    private String name;
    @JdbcTypeCode(SqlTypes.JSON) @Column(columnDefinition = "jsonb", nullable = false) private String meta = "{}";
    @JdbcTypeCode(SqlTypes.JSON) @Column(columnDefinition = "jsonb", name = "last_state") private String lastState;
}

@Entity @Table(name = "sensor_readings")
public class SensorReading {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY) private Long id;
    @ManyToOne(fetch = LAZY) @JoinColumn(name = "node_pk", nullable = false) private Node node;
    private Double temperature; private Double humidity;
    @Column(nullable = false) private Instant ts;
}
```

**INET:** V1 SQL is fixed (roadmap invariant) so the column stays `inet`. Primary approach: `String` attribute + `@JdbcTypeCode(SqlTypes.INET)` (Hibernate 7 binds via PGobject). **Fallback if it fights back during implementation** (fact sheet warns of friction): drop the annotation to plain `String` and append `?stringtype=unspecified` to the JDBC URL so Postgres casts server-side. Decide at implementation time in one commit; both keep the schema untouched.

`V1__init.sql` = the roadmap SQL verbatim (nodes, capabilities, sensor_readings, `idx_readings_node_ts`). Nothing added, nothing renamed.

### 3.5 Discovery upsert (`NodeService.upsertFromDiscovery`)

`@Transactional`. Serial MQTT delivery (DirectChannel) means no concurrent upserts, so plain read-modify-write is safe:

1. `nodeRepo.findByNodeId(p.nodeId())` or new `Node`; set room/fwVersion/ip; save. (Do **not** touch `online` here â€” status topic is the only owner.)
2. Reconcile capabilities by key `(type, channel)`: update `name`/`meta` on match (preserving `last_state`), insert missing, **delete** DB rows absent from the payload (node reflashed with fewer channels) via `orphanRemoval`.
3. `meta` construction: deserialize each capability as Jackson 3 `ObjectNode`, `remove("type"/"channel"/"name")`, remaining fields (`kind`, `model`, `interval_s`, future keys) â†’ `node.toString()` â†’ `meta`. Forward-compatible with unknown firmware fields by construction.

`DiscoveryPayload` record uses `@JsonProperty("node_id")` etc. for snake_case (annotations are Jackson-3-valid), with `capabilities` typed as `List<ObjectNode>` for the reason above.

### 3.6 REST layer (`/api/v1`)

All responses wrapped: `record ApiResponse<T>(T data, ErrorBody error)` with `ok(data)` / `error(code,message)` factories.

| Endpoint | Behavior |
|---|---|
| `GET /api/v1/nodes` | all nodes + capabilities (`@EntityGraph` or join-fetch to avoid N+1) â†’ `List<NodeDto>` |
| `GET /api/v1/nodes/{nodeId}` | by business key `node_id`; 404 `NotFoundException` if absent |
| `POST /api/v1/nodes/{nodeId}/relays/{ch}/command` | validate body; verify node exists AND has capability `(relay, ch)` else 404; `mqttGateway.publish("home/%s/%s/relay/%d/set".formatted(node.getRoom(), nodeId, ch), "{\"state\":\"ON\"}")`; **202 Accepted immediately** â€” state-topic-is-truth rule means we never wait for or fake the resulting state |
| `GET /api/v1/nodes/{nodeId}/sensors/latest` | `findFirstByNodeOrderByTsDesc` â†’ `SensorReadingDto` (404 if node unknown, `data:null` if no readings) |
| `GET /api/v1/nodes/{nodeId}/sensors/history?from=&to=&bucket=` | raw rows, `from` default nowâˆ’24 h, `to` default now (ISO-8601 instants), ordered asc, capped at 10 000 rows via `Pageable`; `bucket` accepted-and-ignored with a log line (Phase 1 contract: raw only) |

DTOs (records):

```java
public record CapabilityDto(String type, int channel, String name,
        @JsonRawValue String meta, @JsonRawValue String lastState) {}
public record NodeDto(String nodeId, String room, String fwVersion, String ip,
        boolean online, Instant lastSeen, List<CapabilityDto> capabilities) {}
public record RelayCommandRequest(@NotNull @Pattern(regexp = "ON|OFF") String state) {}
public record SensorReadingDto(Double temperature, Double humidity, Instant ts) {}
```

`GlobalExceptionHandler` (`@RestControllerAdvice`): `NotFoundException`â†’404, `MethodArgumentNotValidException`â†’400 with field messages, `MethodArgumentTypeMismatchException` (bad instants)â†’400, catch-allâ†’500, all as `ApiResponse.error`.

### 3.7 WebSocket / STOMP (`WebSocketConfig`, `StatePublisher`)

```java
@Configuration @EnableWebSocketMessageBroker
public class WebSocketConfig implements WebSocketMessageBrokerConfigurer {
    public void registerStompEndpoints(StompEndpointRegistry r) {
        r.addEndpoint("/ws").setAllowedOriginPatterns("*").withSockJS();  // "*" dev-only, Phase 2 tightens
    }
    public void configureMessageBroker(MessageBrokerRegistry r) {
        r.enableSimpleBroker("/topic");
        r.setApplicationDestinationPrefixes("/app");   // unused Phase 1, harmless
    }
}
```

**Jackson 3 implication:** Boot 4 auto-configures the STOMP payload converter as `org.springframework.messaging.converter.JacksonJsonMessageConverter` (Jackson 3) â€” nothing to do *unless* you override converters; if you ever do, use that class, never `MappingJackson2MessageConverter`. `JacksonConfig` contributes one `JsonMapperBuilderCustomizer` bean (disable `FAIL_ON_UNKNOWN_PROPERTIES` for firmware payload evolution). Note Jackson 3 default already writes `java.time` as ISO-8601; the event `ts` is a raw `long` epoch-millis so no date config matters there.

```java
public record EventMessage(String type, String nodeId, Integer channel, JsonNode data, long ts) {}

@Component @RequiredArgsConstructor
public class StatePublisher {
    private final SimpMessagingTemplate template;
    public void publish(String type, String nodeId, Integer channel, JsonNode data) {
        template.convertAndSend("/topic/events",
            new EventMessage(type, nodeId, channel, data, System.currentTimeMillis()));
    }
}
```
Types: `RELAY_STATE`, `SENSOR_STATE`, `NODE_STATUS` â€” matching the roadmap payload contract exactly.

### 3.8 `server/Dockerfile` + docker-only dev loop

```dockerfile
FROM maven:3.9.11-eclipse-temurin-21 AS build
WORKDIR /build
COPY pom.xml .
RUN mvn -B -q dependency:go-offline          # cached layer while pom.xml unchanged
COPY src ./src
RUN mvn -B -q package -DskipTests

FROM eclipse-temurin:21-jre-alpine
RUN addgroup -S app && adduser -S app -G app
USER app
WORKDIR /app
COPY --from=build /build/target/*.jar app.jar
EXPOSE 8080
ENTRYPOINT ["java","-jar","app.jar"]
```

Dev loop without host Java â€” a `mvn` utility service in compose under a `tools` profile (shared `.m2` named volume makes repeat runs fast):

```
docker compose --profile tools run --rm mvn test        # unit tests
docker compose --profile tools run --rm mvn verify      # full check
docker compose up -d --build server                     # rebuild+restart backend
```

## 4. `deploy/` design

### 4.1 `docker-compose.yml` (sketch)

```yaml
services:
  postgres:
    image: postgres:16
    environment:
      POSTGRES_DB: ${POSTGRES_DB}
      POSTGRES_USER: ${POSTGRES_USER}
      POSTGRES_PASSWORD: ${POSTGRES_PASSWORD}
    volumes: [ pgdata:/var/lib/postgresql/data ]
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U ${POSTGRES_USER} -d ${POSTGRES_DB}"]
      interval: 5s
      retries: 10

  mosquitto:
    image: eclipse-mosquitto:2
    ports: [ "1883:1883" ]                     # LAN-exposed, no TLS (Phase 1)
    volumes:
      - ./mosquitto/mosquitto.conf:/mosquitto/config/mosquitto.conf:ro
      - ./scripts:/scripts:ro                  # fake-node scripts, exec'd in-container
      - mosquitto-data:/mosquitto/data
      - mosquitto-passwd:/mosquitto/passwd     # named volume: dodges Windows bind-mount perm warnings, keeps secrets off the repo tree

  server:
    build: ../server
    environment:
      DB_URL: jdbc:postgresql://postgres:5432/${POSTGRES_DB}
      DB_USER: ${POSTGRES_USER}
      DB_PASSWORD: ${POSTGRES_PASSWORD}
      MQTT_URI: tcp://mosquitto:1883
      MQTT_USERNAME: ${MQTT_SERVER_USERNAME}
      MQTT_PASSWORD: ${MQTT_SERVER_PASSWORD}
    ports: [ "8080:8080" ]                     # direct API access for dev/verification
    depends_on:
      postgres: { condition: service_healthy }
      mosquitto: { condition: service_started }
    healthcheck:
      test: ["CMD", "wget", "-qO-", "http://localhost:8080/actuator/health"]   # alpine has busybox wget, no curl
      interval: 10s
      retries: 10

  webui:
    build: ../server-webui                     # note: roadmap says webui/, actual dir is server-webui/
    volumes: [ ./nginx/default.conf:/etc/nginx/conf.d/default.conf:ro ]  # tweak proxy w/o rebuild
    ports: [ "${WEBUI_PORT:-80}:80" ]
    depends_on: [ server ]

  mvn:                                         # dev-only build tool, no host Java
    profiles: [ "tools" ]
    image: maven:3.9.11-eclipse-temurin-21
    working_dir: /workspace
    volumes: [ "../server:/workspace", "m2cache:/root/.m2" ]
    entrypoint: [ "mvn" ]
    command: [ "test" ]

volumes: { pgdata: {}, mosquitto-data: {}, mosquitto-passwd: {}, m2cache: {} }
```

### 4.2 `mosquitto/mosquitto.conf`

```
listener 1883
allow_anonymous false
password_file /mosquitto/passwd/passwd
persistence true
persistence_location /mosquitto/data/
log_dest stdout
```
(Mosquitto 2.x binds localhost-only with no config â€” the explicit `listener 1883` is what exposes it; `allow_anonymous false` must also be explicit once a listener is declared.)

### 4.3 Password-file bootstrap (chicken-and-egg: broker exits if `password_file` is missing)

One-time init **before first `up`** (Git Bash; `$MQTT_SERVER_PASSWORD` from `.env`):

```bash
docker compose run --rm --entrypoint sh mosquitto -c \
  "touch /mosquitto/passwd/passwd && chmod 600 /mosquitto/passwd/passwd && \
   mosquitto_passwd -b /mosquitto/passwd/passwd server $MQTT_SERVER_PASSWORD && \
   mosquitto_passwd -b /mosquitto/passwd/passwd node-esp32s3-aabbcc fakenodepw"
docker compose up -d
```

Adding a **real** node user later (node_id known only after flashing â€” MAC-derived):

```bash
docker compose exec mosquitto mosquitto_passwd -b /mosquitto/passwd/passwd node-esp32s3-XXXXXX <password>
docker compose kill -s SIGHUP mosquitto     # live reload of password file, no restart
```
Both commands go in `deploy/README.md` as the documented operator flow. No ACL in Phase 1 (per spec) â€” any authenticated user can pub/sub anything, which is also why the fake-node scripts may run under either user.

### 4.4 `.env.example` (real `.env` gitignored via `deploy/.gitignore`)

```
POSTGRES_DB=smarthome
POSTGRES_USER=smarthome
POSTGRES_PASSWORD=change-me-db
MQTT_SERVER_USERNAME=server
MQTT_SERVER_PASSWORD=change-me-mqtt
WEBUI_PORT=80
```

### 4.5 `nginx/default.conf` â€” same-origin design **confirmed**

Serving the SPA and proxying `/api` + `/ws` through one origin is the right call: zero CORS in production mode, one LAN URL (`http://<host>/`), and WebSocket/SockJS work without cross-origin handshake headaches. CORS-open on the backend remains only for the Vite dev-server workflow.

```nginx
server {
    listen 80;
    root /usr/share/nginx/html;
    index index.html;

    location /api/ {
        proxy_pass http://server:8080;          # no URI part -> full path preserved
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
    location /ws {                              # prefix match covers /ws/info, /ws/websocket, SockJS xhr fallbacks
        proxy_pass http://server:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_read_timeout 3600s;
        proxy_send_timeout 3600s;
    }
    location / { try_files $uri $uri/ /index.html; }   # SPA history-mode fallback
}
```

`server-webui/Dockerfile` (webui workstream, minimal): `node:26-alpine` â†’ `npm ci && npm run build` â†’ `nginx:alpine` + `COPY dist /usr/share/nginx/html` (conf comes from the compose bind mount).

## 5. Ordered implementation steps (each with a smoke test)

1. **`deploy/` skeleton: postgres + mosquitto only** (compose, mosquitto.conf, .env/.env.example, passwd bootstrap). *Smoke:* in-container `mosquitto_sub`/`mosquitto_pub` round-trip with the `server` user; anonymous pub must be **refused**; `docker compose exec postgres psql -U smarthome -c '\l'`.
2. **Server skeleton**: pom.xml, Application class, application.yml, `V1__init.sql`, Dockerfile, .dockerignore; add `server` + `mvn` services to compose. *Smoke:* `docker compose up -d --build server` â†’ `wget`/curl `:8080/actuator/health` = UP; `psql -c '\d nodes'` shows Flyway-created tables + `flyway_schema_history`.
3. **Entities + repositories + read-only REST** (`GET /nodes`, `GET /nodes/{id}`, ApiResponse, exception handler). *Smoke:* `curl /api/v1/nodes` â†’ `{"data":[],"error":null}`; unknown node â†’ 404 envelope. Also proves `ddl-auto: validate` agrees with V1 (catches INET/JSONB mapping mistakes at boot â€” decide the INET fallback here if needed).
4. **MQTT inbound: discovery + status** (MqttConfig, TopicParser, MqttInboundHandler, NodeService upsert). *Smoke:* run `fake-node-boot.sh` (Â§6), then `GET /nodes` shows esp32s3-aabbcc online with 3 capabilities; re-run with a modified discovery (rename channel 2) â†’ row updated not duplicated; restart server â†’ retained replay repopulates.
5. **Relay state + sensor persistence.** *Smoke:* publish relay/sensor states; `GET /nodes` shows `lastState`; `psql -c 'select * from sensor_readings'` shows a row.
6. **WebSocket: WebSocketConfig + StatePublisher + wiring into handler.** *Smoke:* `ws-watch.mjs` running, publish sensor state â†’ `SENSOR_STATE` event printed.
7. **Outbound: MqttGateway + POST command endpoint.** *Smoke:* `watch-node-commands.sh` in one terminal, POST command â†’ `{"state":"ON"}` arrives on `.../relay/1/set`; verify `GET /nodes` `lastState` did **not** change (state-topic-is-truth honored) until fake node replies.
8. **Telemetry endpoints** latest + history. *Smoke:* curl both, with and without `from`/`to`.
9. **webui + nginx same-origin wiring** (compose service + default.conf; SPA build from webui workstream). *Smoke:* `http://localhost/` loads, `http://localhost/api/v1/nodes` proxies, ws-watch pointed at `ws://localhost/ws/websocket` connects.
10. **Full E2E run** (Â§6 script order) + `deploy/README.md` runbook + Conventional Commits throughout (`feat(server): ...`, `feat(deploy): ...`).

## 6. Verification runbook (no hardware, no host Java)

Fake node: `esp32s3-aabbcc`, room `phong-khach`. All MQTT commands execute **inside** the mosquitto container via the bind-mounted scripts â€” this sidesteps PowerShell 5.1's notorious mangling of embedded JSON quotes entirely. Run the `docker compose ...` lines from Git Bash or PowerShell; the JSON lives in the scripts.

`scripts/fake-node-boot.sh` (`docker compose exec mosquitto sh /scripts/fake-node-boot.sh`):

```sh
#!/bin/sh
U="-u server -P ${MQTT_SERVER_PASSWORD:-change-me-mqtt}"
B="home/phong-khach/esp32s3-aabbcc"
mosquitto_pub $U -t "$B/status" -m online -q 1 -r
mosquitto_pub $U -t "$B/discovery" -q 1 -r -m '{"node_id":"esp32s3-aabbcc","room":"phong-khach","fw_version":"1.0.0","ip":"192.168.1.51","capabilities":[{"type":"relay","channel":1,"name":"Den tran"},{"type":"relay","channel":2,"name":"Den ban"},{"type":"sensor","channel":1,"kind":"temperature_humidity","model":"SHT31","interval_s":30}]}'
mosquitto_pub $U -t "$B/relay/1/state" -q 1 -r -m '{"state":"OFF","source":"boot"}'
mosquitto_pub $U -t "$B/relay/2/state" -q 1 -r -m '{"state":"OFF","source":"boot"}'
```

`scripts/fake-node-sensor.sh` â€” one reading (or `while true; do ...; sleep 30; done` loop mode via arg):
```sh
mosquitto_pub $U -t "$B/sensor/state" -q 0 -m "{\"temperature\":28.5,\"humidity\":65.2,\"ts\":$(date +%s)}"
```

`scripts/fake-node-relay.sh ON 1` â€” simulates the node *executing* a command:
```sh
mosquitto_pub $U -t "$B/relay/${2:-1}/state" -q 1 -r -m "{\"state\":\"${1:-ON}\",\"source\":\"mqtt\"}"
```

`scripts/watch-node-commands.sh` â€” see server-issued commands:
```sh
mosquitto_sub $U -v -t "home/phong-khach/esp32s3-aabbcc/relay/+/set" -t "home/phong-khach/esp32s3-aabbcc/cmd"
```

Simulating death-by-LWT (retained offline): `mosquitto_pub $U -t "$B/status" -m offline -q 1 -r`.

**REST checks** (Git Bash; PowerShell users: `curl.exe`, or `Invoke-RestMethod` for the POST to avoid quote mangling):

```bash
curl http://localhost:8080/api/v1/nodes
curl http://localhost:8080/api/v1/nodes/esp32s3-aabbcc
curl -X POST http://localhost:8080/api/v1/nodes/esp32s3-aabbcc/relays/1/command \
     -H 'Content-Type: application/json' -d '{"state":"ON"}'          # expect 202
curl http://localhost:8080/api/v1/nodes/esp32s3-aabbcc/sensors/latest
curl "http://localhost:8080/api/v1/nodes/esp32s3-aabbcc/sensors/history?from=2026-07-26T00:00:00Z&to=2026-07-27T00:00:00Z"
```
```powershell
Invoke-RestMethod -Method Post -Uri http://localhost:8080/api/v1/nodes/esp32s3-aabbcc/relays/1/command -ContentType 'application/json' -Body '{"state":"ON"}'
```

**Watching `/topic/events`** â€” host Node 26 (native `WebSocket`, so `@stomp/stompjs` works without shims; raw-WebSocket SockJS transport lives at `{endpoint}/websocket`). `scripts/ws-watch.mjs`, after `npm i` in `deploy/scripts/`:

```js
import { Client } from '@stomp/stompjs';
const url = process.argv[2] ?? 'ws://localhost:8080/ws/websocket';
const c = new Client({
  brokerURL: url,
  onConnect: () => { console.log('connected', url);
    c.subscribe('/topic/events', m => console.log(new Date().toISOString(), m.body)); },
  onStompError: f => console.error('STOMP error', f.headers, f.body),
});
c.activate();
```

**Full E2E sequence:** `fake-node-boot.sh` â†’ `GET /nodes` (online, 3 caps) â†’ start `ws-watch.mjs` + `watch-node-commands.sh` â†’ `POST .../relays/1/command {"state":"ON"}` â†’ 202 + command visible on `/set` sub + **no** state change yet â†’ `fake-node-relay.sh ON 1` â†’ `RELAY_STATE` event on WS + `lastState` updated in `GET /nodes` â†’ `fake-node-sensor.sh` â†’ `SENSOR_STATE` event + `sensors/latest` returns it â†’ publish retained `offline` â†’ `NODE_STATUS` event + `online:false`. That sequence exercises every roadmap invariant including "server never infers state from sent commands".

## 7. Spring Boot 4 risks/gotchas (recap, inline refs above)

1. **Starter renames** â€” `spring-boot-starter-webmvc`, not `-web` (Â§2). Never copy Boot-3 tutorials' coordinates.
2. **Jackson 3 packages** â€” runtime types are `tools.jackson.*` (`JsonMapper`, `JsonNode`, `DeserializationFeature`); customizer is `JsonMapperBuilderCustomizer`; **annotations stay `com.fasterxml`** (`@JsonProperty`, `@JsonRawValue` fine) (Â§3.3, Â§3.5, Â§3.7).
3. **STOMP converter** â€” auto-configured `JacksonJsonMessageConverter`; only a landmine if converters are overridden manually (Â§3.7).
4. **Hibernate JSON format mapper expects Jackson 2** â†’ JSONB mapped as `String` + `@JsonRawValue` DTO passthrough; revisit with a custom `FormatMapper` only if typed `Map` mapping is ever needed (Â§3.4).
5. **INET column** â€” `@JdbcTypeCode(SqlTypes.INET)` on `String`, fallback `stringtype=unspecified` JDBC URL param; schema untouchable (Â§3.4).
6. **Flyway module split** â€” `flyway-database-postgresql` mandatory alongside `flyway-core` (Â§2).
7. **Paho optional in SI 7.1.0** â€” must declare `org.eclipse.paho.client.mqttv3:1.2.5` explicitly (Â§2).
8. **Two MQTT client IDs** (`server-core` / `server-core-pub`) â€” duplicate IDs cause a broker kick-loop; documented deviation from the roadmap's "single client-id" phrasing (Â§3.2).
9. **Self-echo of `/set` and `/cmd`** under `home/#` â€” must be explicitly ignored in the router (Â§3.3).
10. **Retained-replay ordering** not guaranteed across topics â€” unknown-node states are warn-and-dropped, self-healing via retain semantics (Â§3.3).
11. **Startup race serverâ†”mosquitto** â€” SI adapter `recoveryInterval` retries cover it; `automaticReconnect` alone would not retry a failed *first* connect (Â§3.2).
12. **Mosquitto 2 defaults** â€” explicit `listener 1883` + `allow_anonymous false`; broker exits if `password_file` missing â†’ bootstrap step before first `up`; passwd in a named volume avoids Windows bind-mount permission warnings; `SIGHUP` reloads users (Â§4.2â€“4.3).
13. **Windows shell quoting** â€” JSON never passes through PowerShell 5.1 argv: scripts-in-container for MQTT, Git Bash or `Invoke-RestMethod` for curl (Â§6).
14. **`21-jre-alpine` has no curl** â€” compose healthcheck uses busybox `wget` (Â§4.1).
15. **springdoc 3.0.3** â€” compatible but not yet rebuilt against Boot 4.1; kept out of Phase 1 (Â§2).

### Critical Files for Implementation
- C:\Users\Izuki\Projects\SmartHomeController\server\pom.xml
- C:\Users\Izuki\Projects\SmartHomeController\server\src\main\java\com\smarthome\server\config\MqttConfig.java
- C:\Users\Izuki\Projects\SmartHomeController\server\src\main\java\com\smarthome\server\mqtt\MqttInboundHandler.java
- C:\Users\Izuki\Projects\SmartHomeController\deploy\docker-compose.yml
- C:\Users\Izuki\Projects\SmartHomeController\server\src\main\resources\db\migration\V1__init.sql