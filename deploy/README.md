# deploy/ - server stack (Docker Compose)

Services: `eclipse-mosquitto:2`, `server` (Spring Boot, built from `../server`), and `webui` (Vue build served by nginx, built from `../webui`). An optional `postgres:16` service is available through the `embedded-db` profile.

## First-time setup

```bash
cd deploy
cp .env.example .env        # then edit the passwords
```

**Bootstrap the Mosquitto password file** (one-time; the broker refuses to start without it). Note the `chown` — the bootstrap shell runs as root but the broker drops to user `mosquitto`:

```bash
docker compose run --rm --entrypoint sh mosquitto -c 'touch /mosquitto/passwd/passwd && chmod 600 /mosquitto/passwd/passwd && mosquitto_passwd -b /mosquitto/passwd/passwd "$MQTT_SERVER_USERNAME" "$MQTT_SERVER_PASSWORD" && mosquitto_passwd -b /mosquitto/passwd/passwd node-esp32s3-aabbcc fakenodepw && chown -R mosquitto:mosquitto /mosquitto/passwd'
```

Then:

```bash
docker compose up -d
```

The Web UI, API, WebSocket, and health endpoint share `HTTP_PORT` (port `80` by default):

- Web UI: `http://localhost/`
- API: `http://localhost/api/v1`
- WebSocket: `http://localhost/ws`
- Health: `http://localhost/actuator/health`
- MQTT: port `1883` (LAN, user/password, no TLS in Phase 1)

## External PostgreSQL

To run without the bundled PostgreSQL container, edit `.env`:

```dotenv
COMPOSE_PROFILES=
DB_URL=jdbc:postgresql://database.example.com:5432/smarthome
DB_USER=smarthome
DB_PASSWORD=change-me-external-db
```

Then run `docker compose up -d` normally. The database must be reachable from the `server` container. Flyway creates or updates the application schema when the server starts.

## Adding a real node's MQTT user

After flashing a node you'll know its MAC-derived id (`esp32s3-xxxxxx`):

```bash
docker compose exec mosquitto mosquitto_passwd -b /mosquitto/passwd/passwd node-esp32s3-xxxxxx <password>
docker compose kill -s SIGHUP mosquitto     # live-reload users, no restart
```

## Fake node (test without hardware)

Scripts are bind-mounted read-only into the mosquitto container; JSON never passes through Windows shell quoting. Fake node = `esp32s3-aabbcc`, room `phong-khach`.

```bash
docker compose exec mosquitto sh /scripts/fake-node-boot.sh      # retained status+discovery+relay states
docker compose exec mosquitto sh /scripts/fake-node-sensor.sh    # one reading ("loop" arg = every 30s)
docker compose exec mosquitto sh /scripts/fake-node-relay.sh ON 1  # node "executes" a command
docker compose exec mosquitto sh /scripts/watch-node-commands.sh # watch .../set and .../cmd (Ctrl-C stops)
docker compose exec mosquitto sh /scripts/node-offline.sh        # simulate LWT (retained offline)
```

Watch the WebSocket events the UI receives (host Node.js):

```bash
cd scripts && npm i && node ws-watch.mjs
```

## Backend dev loop

```bash
docker compose up -d --build server                  # rebuild + restart backend
docker compose logs -f server                        # watch logs
```

## Notes / Phase 1 limitations

- No TLS (Phase 2) and no per-user ACL (Phase 3): any authenticated user can pub/sub any topic.
- The backend connects with two MQTT client ids (`server-core` subscriber, `server-core-pub` publisher) — one shared id would cause a broker kick-loop.
- Retained-message replay order across topics isn't guaranteed by MQTT; the server warn-drops states for unknown nodes and self-heals via retained re-replay.
