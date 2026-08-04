CREATE TABLE app_users (
  id                    BIGINT NOT NULL AUTO_INCREMENT,
  username              VARCHAR(100) NOT NULL,
  username_normalized   VARCHAR(100) GENERATED ALWAYS AS (LOWER(username)) STORED,
  display_name          VARCHAR(200) NOT NULL,
  password_hash         VARCHAR(255) NOT NULL,
  enabled               BIT(1) NOT NULL DEFAULT b'1',
  system_admin          BIT(1) NOT NULL DEFAULT b'0',
  must_change_password  BIT(1) NOT NULL DEFAULT b'1',
  security_version      BIGINT NOT NULL DEFAULT 0,
  created_at            DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  updated_at            DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  PRIMARY KEY (id),
  CONSTRAINT uq_app_users_username_normalized UNIQUE (username_normalized)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

CREATE TABLE permissions (
  code VARCHAR(64) NOT NULL,
  PRIMARY KEY (code)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

INSERT INTO permissions (code) VALUES
  ('NODE_VIEW'),
  ('NODE_CONTROL'),
  ('TELEMETRY_VIEW'),
  ('AUDIT_VIEW');

CREATE TABLE folders (
  id                BIGINT NOT NULL AUTO_INCREMENT,
  parent_id         BIGINT,
  parent_scope      BIGINT GENERATED ALWAYS AS (COALESCE(parent_id, 0)) STORED,
  name              VARCHAR(100) NOT NULL,
  name_normalized   VARCHAR(100) GENERATED ALWAYS AS (LOWER(name)) STORED,
  icon              VARCHAR(100) NOT NULL DEFAULT 'i-lucide-folder',
  template_type     VARCHAR(16),
  template_config   JSON NOT NULL DEFAULT (JSON_OBJECT()),
  sort_order        INT NOT NULL DEFAULT 0,
  created_at        DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  updated_at        DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  PRIMARY KEY (id),
  CONSTRAINT fk_folders_parent FOREIGN KEY (parent_id) REFERENCES folders(id) ON DELETE RESTRICT,
  CONSTRAINT uq_folders_sibling_name UNIQUE (parent_scope, name_normalized),
  CONSTRAINT ck_folders_icon_format CHECK (
    REGEXP_LIKE(icon, '^i-lucide-[a-z0-9-]+$', 'c') AND CHAR_LENGTH(icon) <= 100
  )
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

CREATE TABLE folder_closure (
  ancestor_id    BIGINT NOT NULL,
  descendant_id  BIGINT NOT NULL,
  depth          INT NOT NULL,
  PRIMARY KEY (ancestor_id, descendant_id),
  CONSTRAINT fk_folder_closure_ancestor FOREIGN KEY (ancestor_id) REFERENCES folders(id) ON DELETE CASCADE,
  CONSTRAINT fk_folder_closure_descendant FOREIGN KEY (descendant_id) REFERENCES folders(id) ON DELETE CASCADE,
  CONSTRAINT ck_folder_closure_depth CHECK (depth >= 0),
  INDEX idx_folder_closure_descendant (descendant_id, ancestor_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

CREATE TABLE folder_roles (
  id                BIGINT NOT NULL AUTO_INCREMENT,
  folder_id         BIGINT NOT NULL,
  name              VARCHAR(100) NOT NULL,
  name_normalized   VARCHAR(100) GENERATED ALWAYS AS (LOWER(name)) STORED,
  created_at        DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  updated_at        DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  PRIMARY KEY (id),
  CONSTRAINT fk_folder_roles_folder FOREIGN KEY (folder_id) REFERENCES folders(id) ON DELETE CASCADE,
  CONSTRAINT uq_folder_roles_folder_id_id UNIQUE (folder_id, id),
  CONSTRAINT uq_folder_roles_name UNIQUE (folder_id, name_normalized)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

CREATE TABLE folder_role_permissions (
  role_id          BIGINT NOT NULL,
  permission_code  VARCHAR(64) NOT NULL,
  PRIMARY KEY (role_id, permission_code),
  CONSTRAINT fk_folder_role_permissions_role FOREIGN KEY (role_id) REFERENCES folder_roles(id) ON DELETE CASCADE,
  CONSTRAINT fk_folder_role_permissions_permission FOREIGN KEY (permission_code) REFERENCES permissions(code),
  INDEX idx_folder_role_permissions_permission (permission_code)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

CREATE TABLE folder_memberships (
  id          BIGINT NOT NULL AUTO_INCREMENT,
  user_id     BIGINT NOT NULL,
  folder_id   BIGINT NOT NULL,
  role_id     BIGINT NOT NULL,
  created_at  DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  PRIMARY KEY (id),
  CONSTRAINT uq_folder_memberships_user_folder UNIQUE (user_id, folder_id),
  CONSTRAINT fk_folder_memberships_user FOREIGN KEY (user_id) REFERENCES app_users(id) ON DELETE CASCADE,
  CONSTRAINT fk_folder_memberships_folder FOREIGN KEY (folder_id) REFERENCES folders(id) ON DELETE CASCADE,
  CONSTRAINT fk_folder_memberships_role FOREIGN KEY (folder_id, role_id) REFERENCES folder_roles(folder_id, id),
  INDEX idx_folder_memberships_folder (folder_id),
  INDEX idx_folder_memberships_role (role_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

CREATE TABLE device_types (
  id                BIGINT NOT NULL AUTO_INCREMENT,
  name              VARCHAR(100) NOT NULL,
  name_normalized   VARCHAR(100) GENERATED ALWAYS AS (LOWER(name)) STORED,
  description       VARCHAR(255),
  created_at        DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  PRIMARY KEY (id),
  CONSTRAINT uq_device_types_name UNIQUE (name_normalized)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

INSERT INTO device_types (name, description) VALUES
  ('Đèn', 'Đèn và mạch chiếu sáng'),
  ('Quạt', 'Quạt và thiết bị thông gió'),
  ('Ổ cắm', 'Ổ cắm hoặc tải điện đóng cắt'),
  ('Máy bơm', 'Máy bơm nước'),
  ('Rèm', 'Rèm hoặc cửa có điều khiển'),
  ('Khác', 'Thiết bị điều khiển khác');

CREATE TABLE tags (
  id                BIGINT NOT NULL AUTO_INCREMENT,
  name              VARCHAR(100) NOT NULL,
  name_normalized   VARCHAR(100) GENERATED ALWAYS AS (LOWER(name)) STORED,
  color             VARCHAR(255),
  created_at        DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  PRIMARY KEY (id),
  CONSTRAINT uq_tags_name UNIQUE (name_normalized)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

INSERT INTO folders (name) VALUES ('Chưa phân loại');
INSERT INTO folder_closure (ancestor_id, descendant_id, depth)
SELECT id, id, 0 FROM folders WHERE parent_id IS NULL AND name_normalized = LOWER('Chưa phân loại');

CREATE TABLE nodes (
  id                BIGINT NOT NULL AUTO_INCREMENT,
  node_id           VARCHAR(255) NOT NULL,
  room              VARCHAR(255) NOT NULL,
  discovery_name    VARCHAR(100),
  display_name      VARCHAR(100),
  folder_id         BIGINT NOT NULL,
  fw_version        VARCHAR(32),
  ip                VARCHAR(45),
  online            BIT(1) NOT NULL DEFAULT b'0',
  last_seen         DATETIME(6),
  created_at        DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  approval_status   VARCHAR(16) NOT NULL DEFAULT 'PENDING',
  approved_at       DATETIME(6),
  approved_by       BIGINT,
  PRIMARY KEY (id),
  CONSTRAINT uq_nodes_node_id UNIQUE (node_id),
  CONSTRAINT fk_nodes_folder FOREIGN KEY (folder_id) REFERENCES folders(id) ON DELETE RESTRICT,
  CONSTRAINT fk_nodes_approved_by FOREIGN KEY (approved_by) REFERENCES app_users(id),
  CONSTRAINT ck_nodes_approval_status CHECK (approval_status IN ('PENDING', 'APPROVED', 'REJECTED')),
  CONSTRAINT ck_nodes_approval_metadata CHECK (
    (approval_status = 'APPROVED') = (approved_at IS NOT NULL AND approved_by IS NOT NULL)
  ),
  INDEX idx_nodes_folder (folder_id),
  INDEX idx_nodes_approved_by (approved_by)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

CREATE TABLE capabilities (
  id                BIGINT NOT NULL AUTO_INCREMENT,
  node_pk           BIGINT NOT NULL,
  type              VARCHAR(255) NOT NULL,
  channel           INT NOT NULL,
  discovery_name    VARCHAR(100),
  display_name      VARCHAR(100),
  device_type_id    BIGINT,
  meta              JSON NOT NULL DEFAULT (JSON_OBJECT()),
  last_state        JSON,
  PRIMARY KEY (id),
  CONSTRAINT uq_capabilities_node_type_channel UNIQUE (node_pk, type, channel),
  CONSTRAINT fk_capabilities_node FOREIGN KEY (node_pk) REFERENCES nodes(id) ON DELETE CASCADE,
  CONSTRAINT fk_capabilities_device_type FOREIGN KEY (device_type_id) REFERENCES device_types(id) ON DELETE SET NULL,
  INDEX idx_capabilities_device_type (device_type_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

CREATE TABLE capability_tags (
  capability_id  BIGINT NOT NULL,
  tag_id         BIGINT NOT NULL,
  PRIMARY KEY (capability_id, tag_id),
  CONSTRAINT fk_capability_tags_capability FOREIGN KEY (capability_id) REFERENCES capabilities(id) ON DELETE CASCADE,
  CONSTRAINT fk_capability_tags_tag FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE,
  INDEX idx_capability_tags_tag (tag_id, capability_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

CREATE TABLE sensor_readings (
  id           BIGINT NOT NULL AUTO_INCREMENT,
  node_pk      BIGINT NOT NULL,
  temperature  DOUBLE,
  humidity     DOUBLE,
  ts           DATETIME(6) NOT NULL,
  PRIMARY KEY (id),
  CONSTRAINT fk_sensor_readings_node FOREIGN KEY (node_pk) REFERENCES nodes(id),
  INDEX idx_readings_node_ts (node_pk, ts DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

CREATE TABLE placements (
  id             BIGINT NOT NULL AUTO_INCREMENT,
  folder_id      BIGINT NOT NULL,
  capability_id  BIGINT,
  label          VARCHAR(100),
  x              DOUBLE NOT NULL,
  y              DOUBLE NOT NULL,
  width          DOUBLE NOT NULL DEFAULT 1,
  height         DOUBLE NOT NULL DEFAULT 1,
  sort_order     INT NOT NULL DEFAULT 0,
  config         JSON NOT NULL DEFAULT (JSON_OBJECT()),
  created_at     DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  updated_at     DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  PRIMARY KEY (id),
  CONSTRAINT uq_placements_folder_capability UNIQUE (folder_id, capability_id),
  CONSTRAINT fk_placements_folder FOREIGN KEY (folder_id) REFERENCES folders(id) ON DELETE CASCADE,
  CONSTRAINT fk_placements_capability FOREIGN KEY (capability_id) REFERENCES capabilities(id) ON DELETE CASCADE,
  INDEX idx_placements_folder (folder_id, sort_order, id),
  INDEX idx_placements_capability (capability_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

CREATE TABLE audit_logs (
  id              BIGINT NOT NULL AUTO_INCREMENT,
  actor_user_id   BIGINT,
  action          VARCHAR(64) NOT NULL,
  target_type     VARCHAR(32) NOT NULL,
  target_id       VARCHAR(255),
  details         JSON NOT NULL DEFAULT (JSON_OBJECT()),
  created_at      DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  correlation_id  VARCHAR(36),
  batch_id        VARCHAR(36),
  ip              VARCHAR(45),
  capability_id   BIGINT,
  folder_id       BIGINT,
  PRIMARY KEY (id),
  CONSTRAINT fk_audit_logs_actor FOREIGN KEY (actor_user_id) REFERENCES app_users(id),
  CONSTRAINT fk_audit_logs_capability FOREIGN KEY (capability_id) REFERENCES capabilities(id) ON DELETE SET NULL,
  CONSTRAINT fk_audit_logs_folder FOREIGN KEY (folder_id) REFERENCES folders(id) ON DELETE SET NULL,
  INDEX idx_audit_logs_created_at (created_at DESC),
  INDEX idx_audit_logs_actor (actor_user_id, created_at DESC),
  INDEX idx_audit_logs_correlation (correlation_id),
  INDEX idx_audit_logs_batch (batch_id),
  INDEX idx_audit_logs_folder_created (folder_id, created_at DESC),
  INDEX idx_audit_logs_capability (capability_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;

CREATE TABLE bulk_operations (
  id               BIGINT NOT NULL AUTO_INCREMENT,
  batch_id         VARCHAR(36) NOT NULL,
  actor_user_id    BIGINT NOT NULL,
  idempotency_key  VARCHAR(100) NOT NULL,
  folder_id        BIGINT NOT NULL,
  response         JSON,
  created_at       DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  PRIMARY KEY (id),
  CONSTRAINT uq_bulk_operations_batch_id UNIQUE (batch_id),
  CONSTRAINT uq_bulk_operations_actor_key UNIQUE (actor_user_id, idempotency_key),
  CONSTRAINT fk_bulk_operations_actor FOREIGN KEY (actor_user_id) REFERENCES app_users(id),
  CONSTRAINT fk_bulk_operations_folder FOREIGN KEY (folder_id) REFERENCES folders(id),
  INDEX idx_bulk_operations_folder (folder_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;
