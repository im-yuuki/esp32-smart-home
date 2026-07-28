# Folder tree, maps, bulk control, and audit

## Invariants

- `node_id` and MQTT `room` remain technical identifiers owned by firmware.
- Every node belongs to exactly one folder; permissions assigned to an ancestor apply to descendants.
- Discovery updates `discovery_name`, never the user-managed `display_name`.
- A command response means dispatched, not executed. MQTT state topics remain the source of truth.
- Each control writes `CONTROL_REQUESTED` before publish, followed by `CONTROL_DISPATCHED`, `CONTROL_FAILED`, or `CONTROL_SKIPPED_OFFLINE`.

## Migration

Flyway V3 copies flat groups to folders and creates closure rows. Nodes with zero groups move to `Chưa phân loại`. Nodes with one group move to the corresponding folder. Nodes with several groups receive a dedicated `[Imported] <node_id>` folder; generated per-user roles preserve the union of their prior permissions without widening access to another node.

Legacy group tables remain as migration history but application code no longer reads them.

## Authorization

`NODE_VIEW`, `NODE_CONTROL`, `TELEMETRY_VIEW`, and `AUDIT_VIEW` are resolved from all folder memberships on the target folder's ancestor chain. System administrators bypass folder checks. Browse responses include otherwise-hidden ancestors only to preserve the tree path; those ancestors are not navigable without `NODE_VIEW`.

## Firmware boundary

No firmware change is required. Unicode names, device types, tags, and map coordinates stay in PostgreSQL. This avoids NVS byte limits and prevents display metadata from changing MQTT topics or being overwritten on reconnect.
