CREATE TABLE app_users (
  id                   BIGSERIAL PRIMARY KEY,
  username             TEXT NOT NULL,
  display_name         TEXT NOT NULL,
  password_hash        TEXT NOT NULL,
  enabled              BOOLEAN NOT NULL DEFAULT TRUE,
  system_admin         BOOLEAN NOT NULL DEFAULT FALSE,
  must_change_password BOOLEAN NOT NULL DEFAULT TRUE,
  security_version     BIGINT NOT NULL DEFAULT 0,
  created_at           TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at           TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE UNIQUE INDEX uq_app_users_username_lower ON app_users (lower(username));

ALTER TABLE nodes
  ADD COLUMN approval_status TEXT NOT NULL DEFAULT 'PENDING',
  ADD COLUMN approved_at TIMESTAMPTZ,
  ADD COLUMN approved_by BIGINT REFERENCES app_users(id),
  ADD CONSTRAINT ck_nodes_approval_status
    CHECK (approval_status IN ('PENDING', 'APPROVED', 'REJECTED')),
  ADD CONSTRAINT ck_nodes_approval_metadata
    CHECK ((approval_status = 'APPROVED') = (approved_at IS NOT NULL AND approved_by IS NOT NULL));

CREATE TABLE node_groups (
  id          BIGSERIAL PRIMARY KEY,
  name        TEXT NOT NULL,
  description TEXT,
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE UNIQUE INDEX uq_node_groups_name_lower ON node_groups (lower(name));

CREATE TABLE permissions (
  code TEXT PRIMARY KEY
);
INSERT INTO permissions (code) VALUES
  ('NODE_VIEW'),
  ('NODE_CONTROL'),
  ('TELEMETRY_VIEW');

CREATE TABLE group_roles (
  id         BIGSERIAL PRIMARY KEY,
  group_id   BIGINT NOT NULL REFERENCES node_groups(id) ON DELETE CASCADE,
  name       TEXT NOT NULL,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  UNIQUE (group_id, id)
);
CREATE UNIQUE INDEX uq_group_roles_name_lower ON group_roles (group_id, lower(name));

CREATE TABLE group_role_permissions (
  role_id         BIGINT NOT NULL REFERENCES group_roles(id) ON DELETE CASCADE,
  permission_code TEXT NOT NULL REFERENCES permissions(code),
  PRIMARY KEY (role_id, permission_code)
);

CREATE TABLE group_memberships (
  id         BIGSERIAL PRIMARY KEY,
  user_id    BIGINT NOT NULL REFERENCES app_users(id) ON DELETE CASCADE,
  group_id   BIGINT NOT NULL REFERENCES node_groups(id) ON DELETE CASCADE,
  role_id    BIGINT NOT NULL,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  UNIQUE (user_id, group_id),
  FOREIGN KEY (group_id, role_id) REFERENCES group_roles(group_id, id)
);
CREATE INDEX idx_group_memberships_group ON group_memberships (group_id);
CREATE INDEX idx_group_memberships_role ON group_memberships (role_id);

CREATE TABLE node_group_memberships (
  id         BIGSERIAL PRIMARY KEY,
  node_pk    BIGINT NOT NULL REFERENCES nodes(id) ON DELETE CASCADE,
  group_id   BIGINT NOT NULL REFERENCES node_groups(id) ON DELETE CASCADE,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  UNIQUE (node_pk, group_id)
);
CREATE INDEX idx_node_group_memberships_group ON node_group_memberships (group_id);

CREATE TABLE audit_logs (
  id            BIGSERIAL PRIMARY KEY,
  actor_user_id BIGINT REFERENCES app_users(id),
  action        TEXT NOT NULL,
  target_type   TEXT NOT NULL,
  target_id     TEXT,
  details       JSONB NOT NULL DEFAULT '{}',
  created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX idx_audit_logs_created_at ON audit_logs (created_at DESC);
CREATE INDEX idx_audit_logs_actor ON audit_logs (actor_user_id, created_at DESC);
