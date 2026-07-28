import { http } from './http'
import { isMock, mockApi } from './mock'
import type {
  CapabilityDto,
  NodeDto,
  NodeInfo,
  RelayChannel,
  SensorMeta,
  SensorReading,
  DeviceType,
  Tag,
  CapabilityInfo,
} from '@/types/api'
import { isoToMs, secondsToMs } from '@/utils/time'

// ---------------------------------------------------------------------------
// DTO -> view-model mapping (capabilities[] -> relays[] / sensor)
// ---------------------------------------------------------------------------

/** `meta` / `lastState` arrive as raw JSON values; parse defensively (they may
 *  be objects, JSON strings, or null depending on serializer behavior). */
export function asObject(value: unknown): Record<string, unknown> | null {
  if (value == null) return null
  if (typeof value === 'string') {
    try {
      return asObject(JSON.parse(value))
    } catch {
      return null
    }
  }
  if (typeof value === 'object' && !Array.isArray(value)) {
    return value as Record<string, unknown>
  }
  return null
}

const str = (v: unknown): string | undefined => (typeof v === 'string' ? v : undefined)
const num = (v: unknown): number | undefined =>
  typeof v === 'number' && Number.isFinite(v) ? v : undefined
const id = (v: unknown): string | undefined =>
  typeof v === 'string' || typeof v === 'number' ? String(v) : undefined

function mapDeviceType(value: unknown): DeviceType | null {
  if (typeof value === 'string') return { id: value, name: value }
  const item = asObject(value)
  const itemId = id(item?.id)
  if (!itemId) return null
  return { id: itemId, name: str(item?.name) ?? itemId, icon: str(item?.icon), description: str(item?.description) }
}

function mapTags(value: unknown): Tag[] {
  if (!Array.isArray(value)) return []
  return value.flatMap((raw) => {
    if (typeof raw === 'string') return [{ id: raw, name: raw }]
    const item = asObject(raw)
    const itemId = id(item?.id)
    return itemId ? [{ id: itemId, name: str(item?.name) ?? itemId, color: str(item?.color) }] : []
  })
}

function mapCapability(cap: CapabilityDto, index: number): CapabilityInfo {
  return {
    id: id(cap.id) ?? `${cap.type}:${cap.channel}:${index}`,
    type: cap.type,
    channel: cap.channel,
    discoveryName: cap.discoveryName ?? '',
    displayName: cap.displayName ?? null,
    deviceType: mapDeviceType(cap.deviceType),
    tags: mapTags(cap.tags),
    meta: asObject(cap.meta),
    lastState: asObject(cap.lastState),
  }
}

function mapRelay(cap: CapabilityDto): RelayChannel {
  const last = asObject(cap.lastState)
  const state = last?.state === 'ON' || last?.state === 'OFF' ? last.state : 'UNKNOWN'
  return {
    id: id(cap.id),
    channel: cap.channel,
    name: cap.discoveryName || '',
    discoveryName: cap.discoveryName || '',
    displayName: cap.displayName ?? null,
    deviceType: mapDeviceType(cap.deviceType),
    tags: mapTags(cap.tags),
    state,
    source: str(last?.source),
    pending: false,
  }
}

function mapSensorMeta(cap: CapabilityDto): SensorMeta {
  const meta = asObject(cap.meta)
  return {
    kind: str(meta?.kind),
    model: str(meta?.model),
    intervalS: num(meta?.interval_s),
  }
}

/** Sensor lastState payload is the raw MQTT sensor state:
 *  `{"temperature":..,"humidity":..,"ts":<epoch SECONDS>}` -> ts normalized to ms. */
function mapSensorReading(cap: CapabilityDto, fallbackTsMs: number | null): SensorReading | null {
  const last = asObject(cap.lastState)
  const temperature = num(last?.temperature)
  const humidity = num(last?.humidity)
  if (temperature === undefined || humidity === undefined) return null
  const tsSeconds = num(last?.ts)
  return {
    temperature,
    humidity,
    ts: tsSeconds !== undefined ? secondsToMs(tsSeconds) : (fallbackTsMs ?? Date.now()),
  }
}

export function mapNodeDto(dto: NodeDto): NodeInfo {
  const lastSeen = isoToMs(dto.lastSeen)
  const relays: RelayChannel[] = []
  let hasSensor = false
  let sensorMeta: SensorMeta | undefined
  let sensor: SensorReading | null = null

  const capabilities = (dto.capabilities ?? []).map(mapCapability)
  for (const cap of dto.capabilities ?? []) {
    if (cap.type === 'relay') {
      relays.push(mapRelay(cap))
    } else if (cap.type === 'sensor') {
      hasSensor = true
      sensorMeta = mapSensorMeta(cap)
      sensor = mapSensorReading(cap, lastSeen)
    }
    // Unknown capability types from future firmware: ignored (forward-compatible).
  }
  relays.sort((a, b) => a.channel - b.channel)

  return {
    nodeId: dto.nodeId,
    displayName: dto.displayName ?? null,
    discoveryName: dto.discoveryName ?? null,
    room: dto.room,
    folderId: dto.folderId == null ? null : String(dto.folderId),
    fwVersion: dto.fwVersion ?? undefined,
    ip: dto.ip ?? undefined,
    online: dto.online,
    lastSeen,
    relays,
    hasSensor,
    sensorMeta,
    sensor,
    permissions: dto.permissions ?? [],
    capabilities,
  }
}

// ---------------------------------------------------------------------------
// API functions
// ---------------------------------------------------------------------------

export async function listNodes(): Promise<NodeInfo[]> {
  if (isMock) return mockApi.listNodes()
  const res = await http.get<NodeDto[]>('/nodes')
  return res.data.map(mapNodeDto)
}

export async function getNode(nodeId: string): Promise<NodeInfo> {
  if (isMock) return mockApi.getNode(nodeId)
  const res = await http.get<NodeDto>(`/nodes/${encodeURIComponent(nodeId)}`)
  return mapNodeDto(res.data)
}

/** POST a relay command. Resolves on 202 Accepted — the actual state change
 *  arrives later as a RELAY_STATE event (state topic is the source of truth). */
export async function sendRelayCommand(
  nodeId: string,
  channel: number,
  state: 'ON' | 'OFF',
): Promise<void> {
  if (isMock) return mockApi.sendRelayCommand(nodeId, channel, state)
  await http.post(`/nodes/${encodeURIComponent(nodeId)}/relays/${channel}/command`, { state })
}
