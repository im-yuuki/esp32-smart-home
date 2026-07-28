INSERT INTO permissions (code) VALUES ('AUDIT_VIEW') ON CONFLICT DO NOTHING;

CREATE TABLE folders (
  id              BIGSERIAL PRIMARY KEY,
  parent_id       BIGINT REFERENCES folders(id) ON DELETE RESTRICT,
  name            TEXT NOT NULL,
  template_type   TEXT,
  template_config JSONB NOT NULL DEFAULT '{}',
  sort_order      INT NOT NULL DEFAULT 0,
  created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
  CONSTRAINT ck_folders_not_self_parent CHECK (parent_id IS NULL OR parent_id <> id)
);
CREATE UNIQUE INDEX uq_folders_sibling_name_lower
  ON folders (COALESCE(parent_id, 0), lower(name));

-- Preserve legacy identifiers so role and membership backfill remains deterministic.
INSERT INTO folders (id, name, created_at, updated_at)
SELECT id, name, created_at, updated_at FROM node_groups ORDER BY id;
SELECT setval(pg_get_serial_sequence('folders', 'id'),
              GREATEST(COALESCE((SELECT max(id) FROM folders), 0), 1), true);
INSERT INTO folders (name)
SELECT 'Chưa phân loại'
WHERE NOT EXISTS (SELECT 1 FROM folders WHERE lower(name) = lower('Chưa phân loại'));

-- A legacy node could belong to several authorization groups. A tree permits one folder
-- per node, so give each such node a dedicated import folder. Effective permissions are
-- copied below instead of silently choosing one group and revoking access from the others.
INSERT INTO folders (name)
SELECT '[Imported] ' || n.node_id
FROM nodes n
WHERE (SELECT count(*) FROM node_group_memberships ngm WHERE ngm.node_pk = n.id) > 1
ON CONFLICT DO NOTHING;

CREATE TABLE folder_closure (
  ancestor_id   BIGINT NOT NULL REFERENCES folders(id) ON DELETE CASCADE,
  descendant_id BIGINT NOT NULL REFERENCES folders(id) ON DELETE CASCADE,
  depth         INT NOT NULL CHECK (depth >= 0),
  PRIMARY KEY (ancestor_id, descendant_id)
);
CREATE INDEX idx_folder_closure_descendant ON folder_closure (descendant_id, ancestor_id);
INSERT INTO folder_closure (ancestor_id, descendant_id, depth)
SELECT id, id, 0 FROM folders;

ALTER TABLE nodes
  ADD COLUMN folder_id BIGINT,
  ADD COLUMN display_name TEXT,
  ADD COLUMN discovery_name TEXT;
UPDATE nodes n
SET folder_id = CASE
  WHEN (SELECT count(*) FROM node_group_memberships ngm WHERE ngm.node_pk = n.id) > 1
    THEN (SELECT id FROM folders WHERE name = '[Imported] ' || n.node_id)
  ELSE COALESCE(
    (SELECT min(ngm.group_id) FROM node_group_memberships ngm WHERE ngm.node_pk = n.id),
    (SELECT min(id) FROM folders WHERE lower(name) = lower('Chưa phân loại'))
  )
END, discovery_name = n.node_id;
ALTER TABLE nodes
  ALTER COLUMN folder_id SET NOT NULL,
  ADD CONSTRAINT fk_nodes_folder FOREIGN KEY (folder_id) REFERENCES folders(id) ON DELETE RESTRICT;
CREATE INDEX idx_nodes_folder ON nodes (folder_id);

CREATE TABLE folder_roles (
  id         BIGSERIAL PRIMARY KEY,
  folder_id  BIGINT NOT NULL REFERENCES folders(id) ON DELETE CASCADE,
  name       TEXT NOT NULL,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  UNIQUE (folder_id, id)
);
CREATE UNIQUE INDEX uq_folder_roles_name_lower ON folder_roles (folder_id, lower(name));
INSERT INTO folder_roles (id, folder_id, name, created_at, updated_at)
SELECT id, group_id, name, created_at, updated_at FROM group_roles ORDER BY id;
SELECT setval(pg_get_serial_sequence('folder_roles', 'id'),
              GREATEST(COALESCE((SELECT max(id) FROM folder_roles), 0), 1), true);

CREATE TABLE folder_role_permissions (
  role_id         BIGINT NOT NULL REFERENCES folder_roles(id) ON DELETE CASCADE,
  permission_code TEXT NOT NULL REFERENCES permissions(code),
  PRIMARY KEY (role_id, permission_code)
);
INSERT INTO folder_role_permissions (role_id, permission_code)
SELECT role_id, permission_code FROM group_role_permissions;

CREATE TABLE folder_memberships (
  id         BIGSERIAL PRIMARY KEY,
  user_id    BIGINT NOT NULL REFERENCES app_users(id) ON DELETE CASCADE,
  folder_id  BIGINT NOT NULL REFERENCES folders(id) ON DELETE CASCADE,
  role_id    BIGINT NOT NULL,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  UNIQUE (user_id, folder_id),
  FOREIGN KEY (folder_id, role_id) REFERENCES folder_roles(folder_id, id)
);
CREATE INDEX idx_folder_memberships_folder ON folder_memberships (folder_id);
CREATE INDEX idx_folder_memberships_role ON folder_memberships (role_id);
INSERT INTO folder_memberships (user_id, folder_id, role_id, created_at)
SELECT user_id, group_id, role_id, created_at FROM group_memberships;

-- Preserve the union of each user's effective permissions on legacy multi-group nodes.
-- Imported folders contain one node, so these generated roles cannot widen access to a
-- different node while still fitting the new one-folder-per-node model.
INSERT INTO folder_roles (folder_id, name)
SELECT n.folder_id, 'Imported access: ' || u.username
FROM nodes n
JOIN node_group_memberships ngm ON ngm.node_pk = n.id
JOIN group_memberships gm ON gm.group_id = ngm.group_id
JOIN app_users u ON u.id = gm.user_id
WHERE (SELECT count(*) FROM node_group_memberships memberships WHERE memberships.node_pk = n.id) > 1
GROUP BY n.folder_id, u.username;

INSERT INTO folder_role_permissions (role_id, permission_code)
SELECT DISTINCT fr.id, grp.permission_code
FROM nodes n
JOIN node_group_memberships ngm ON ngm.node_pk = n.id
JOIN group_memberships gm ON gm.group_id = ngm.group_id
JOIN app_users u ON u.id = gm.user_id
JOIN group_role_permissions grp ON grp.role_id = gm.role_id
JOIN folder_roles fr ON fr.folder_id = n.folder_id
  AND fr.name = 'Imported access: ' || u.username
WHERE (SELECT count(*) FROM node_group_memberships memberships WHERE memberships.node_pk = n.id) > 1;

INSERT INTO folder_memberships (user_id, folder_id, role_id)
SELECT DISTINCT u.id, n.folder_id, fr.id
FROM nodes n
JOIN node_group_memberships ngm ON ngm.node_pk = n.id
JOIN group_memberships gm ON gm.group_id = ngm.group_id
JOIN app_users u ON u.id = gm.user_id
JOIN folder_roles fr ON fr.folder_id = n.folder_id
  AND fr.name = 'Imported access: ' || u.username
WHERE (SELECT count(*) FROM node_group_memberships memberships WHERE memberships.node_pk = n.id) > 1;

CREATE TABLE device_types (
  id          BIGSERIAL PRIMARY KEY,
  name        TEXT NOT NULL,
  description TEXT,
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE UNIQUE INDEX uq_device_types_name_lower ON device_types (lower(name));
INSERT INTO device_types (name, description) VALUES
  ('Đèn', 'Đèn và mạch chiếu sáng'),
  ('Quạt', 'Quạt và thiết bị thông gió'),
  ('Ổ cắm', 'Ổ cắm hoặc tải điện đóng cắt'),
  ('Máy bơm', 'Máy bơm nước'),
  ('Rèm', 'Rèm hoặc cửa có điều khiển'),
  ('Khác', 'Thiết bị điều khiển khác');

CREATE TABLE tags (
  id         BIGSERIAL PRIMARY KEY,
  name       TEXT NOT NULL,
  color      TEXT,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE UNIQUE INDEX uq_tags_name_lower ON tags (lower(name));

ALTER TABLE capabilities
  ADD COLUMN discovery_name TEXT,
  ADD COLUMN display_name TEXT,
  ADD COLUMN device_type_id BIGINT REFERENCES device_types(id) ON DELETE SET NULL;
UPDATE capabilities SET discovery_name = name;
CREATE INDEX idx_capabilities_device_type ON capabilities (device_type_id);

CREATE TABLE capability_tags (
  capability_id BIGINT NOT NULL REFERENCES capabilities(id) ON DELETE CASCADE,
  tag_id        BIGINT NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
  PRIMARY KEY (capability_id, tag_id)
);
CREATE INDEX idx_capability_tags_tag ON capability_tags (tag_id, capability_id);

CREATE TABLE placements (
  id            BIGSERIAL PRIMARY KEY,
  folder_id     BIGINT NOT NULL REFERENCES folders(id) ON DELETE CASCADE,
  capability_id BIGINT REFERENCES capabilities(id) ON DELETE CASCADE,
  label         TEXT,
  x             DOUBLE PRECISION NOT NULL,
  y             DOUBLE PRECISION NOT NULL,
  width         DOUBLE PRECISION NOT NULL DEFAULT 1,
  height        DOUBLE PRECISION NOT NULL DEFAULT 1,
  sort_order    INT NOT NULL DEFAULT 0,
  config        JSONB NOT NULL DEFAULT '{}',
  created_at    TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX idx_placements_folder ON placements (folder_id, sort_order, id);
CREATE UNIQUE INDEX uq_placements_folder_capability
  ON placements (folder_id, capability_id) WHERE capability_id IS NOT NULL;

ALTER TABLE audit_logs
  ADD COLUMN correlation_id TEXT,
  ADD COLUMN batch_id TEXT,
  ADD COLUMN ip INET,
  ADD COLUMN capability_id BIGINT REFERENCES capabilities(id) ON DELETE SET NULL,
  ADD COLUMN folder_id BIGINT REFERENCES folders(id) ON DELETE SET NULL;
CREATE INDEX idx_audit_logs_correlation ON audit_logs (correlation_id);
CREATE INDEX idx_audit_logs_batch ON audit_logs (batch_id);
CREATE INDEX idx_audit_logs_folder_created ON audit_logs (folder_id, created_at DESC);

CREATE TABLE bulk_operations (
  id              BIGSERIAL PRIMARY KEY,
  batch_id        TEXT NOT NULL UNIQUE,
  actor_user_id   BIGINT NOT NULL REFERENCES app_users(id),
  idempotency_key TEXT NOT NULL,
  folder_id       BIGINT NOT NULL REFERENCES folders(id),
  response         JSONB,
  created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
  UNIQUE (actor_user_id, idempotency_key)
);
