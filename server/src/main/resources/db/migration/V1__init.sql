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
