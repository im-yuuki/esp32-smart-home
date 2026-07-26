/**
 * Raw backend DTO shapes (`/api/v1`, ApiResponse envelope already unwrapped by api/http.ts)
 * and the view models the rest of the app consumes.
 *
 * Timestamp rule (cross-plan reconciliation): ALL timestamps are normalized to
 * **epoch milliseconds** at the api/event boundary. Components never convert.
 *  - REST `lastSeen` / history row `ts` arrive as ISO-8601 strings  -> Date.parse
 *  - WS event envelope `ts` is already epoch ms                     -> pass through
 *  - MQTT sensor payload `ts` (inside capability lastState) is epoch seconds -> x1000
 */

// ---------------------------------------------------------------------------
// Backend DTOs (what the wire actually carries)
// ---------------------------------------------------------------------------

/** Backend CapabilityDto. `meta` / `lastState` are raw JSON values re-emitted
 *  by the server via @JsonRawValue — objects on the wire, typed `unknown` here
 *  and parsed defensively in api/nodes.ts. */
export interface CapabilityDto {
  type: string // 'relay' | 'sensor'
  channel: number
  name: string | null
  meta: unknown
  lastState: unknown
}

/** Backend NodeDto (GET /api/v1/nodes, GET /api/v1/nodes/{nodeId}). */
export interface NodeDto {
  nodeId: string
  room: string
  fwVersion: string | null
  ip: string | null
  online: boolean
  lastSeen: string | null // ISO-8601 instant
  capabilities: CapabilityDto[] | null
}

/** Backend SensorReadingDto (GET .../sensors/history rows). */
export interface SensorReadingDto {
  temperature: number | null
  humidity: number | null
  ts: string // ISO-8601 instant
}

// ---------------------------------------------------------------------------
// View models (everything below uses epoch ms)
// ---------------------------------------------------------------------------

export type RelayState = 'ON' | 'OFF' | 'UNKNOWN'

export interface RelayChannel {
  channel: number
  name: string
  state: RelayState
  source?: string
  /** True while a command for this channel is awaiting its RELAY_STATE ack. */
  pending: boolean
}

export interface SensorReading {
  temperature: number
  humidity: number
  ts: number // epoch ms
}

export interface SensorMeta {
  kind?: string
  model?: string
  intervalS?: number
}

export interface NodeInfo {
  nodeId: string
  room: string
  fwVersion?: string
  ip?: string
  online: boolean
  lastSeen: number | null // epoch ms
  relays: RelayChannel[]
  hasSensor: boolean
  sensorMeta?: SensorMeta
  sensor: SensorReading | null
}

export interface SensorSample {
  ts: number // epoch ms
  temperature: number
  humidity: number
}
