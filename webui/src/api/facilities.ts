import { http } from './http'
import { isMock } from './mock'
import { mapNodeDto } from './nodes'
import { mockNodes } from './mock/fixtures'
import type { NodeDto, DeviceType, Tag } from '@/types/api'
import type {
  AuditFilters, AuditLog, BulkActionPayload, BulkActionResult, CapabilityPlacement, FolderDto, FolderMap,
  PageResult,
} from '@/types/facility'

const object = (value: unknown): Record<string, unknown> =>
  value && typeof value === 'object' && !Array.isArray(value) ? value as Record<string, unknown> : {}
const string = (value: unknown, fallback = ''): string => value == null ? fallback : String(value)
const number = (value: unknown, fallback = 0): number => typeof value === 'number' && Number.isFinite(value) ? value : fallback
const list = (value: unknown): unknown[] => Array.isArray(value) ? value : []
const jsonObject = (value: unknown): Record<string, unknown> => {
  if (typeof value === 'string') {
    try { return object(JSON.parse(value)) } catch { return {} }
  }
  return object(value)
}

export function mapFolder(raw: unknown): FolderDto {
  const value = object(raw)
  const config = jsonObject(value.templateConfig)
  const type = string(value.templateType ?? value.template, 'ROOM').toUpperCase()
  return {
    id: string(value.id),
    parentId: value.parentId == null ? null : string(value.parentId),
    name: string(value.name, string(value.id)),
    templateType: ['OUTDOOR', 'BUILDING', 'FLOOR', 'CORRIDOR', 'ROOM'].includes(type) ? type as FolderDto['templateType'] : 'ROOM',
    templateConfig: config,
    sortOrder: number(value.sortOrder),
    permissions: list(value.permissions).filter((item): item is string => typeof item === 'string'),
  }
}

function mapCatalogItem(raw: unknown): { id: string; name: string; icon?: string; description?: string; color?: string } | null {
  if (typeof raw === 'string') return { id: raw, name: raw }
  const value = object(raw)
  if (value.id == null) return null
  return { id: string(value.id), name: string(value.name, string(value.id)), icon: typeof value.icon === 'string' ? value.icon : undefined, description: typeof value.description === 'string' ? value.description : undefined, color: typeof value.color === 'string' ? value.color : undefined }
}

function mapBulk(raw: unknown): BulkActionResult {
  const value = object(raw)
  const results = list(value.results).map((rawResult) => {
    const result = object(rawResult)
    return {
      capabilityId: string(result.capabilityId),
      nodeId: string(result.nodeId),
      channel: number(result.channel),
      online: result.online === true,
      status: string(result.status, 'UNKNOWN'),
    }
  })
  const matched = number(value.matched, results.length)
  const skipped = number(value.skipped)
  return {
    batchId: value.batchId == null ? null : string(value.batchId),
    matched,
    dispatchable: number(value.dispatchable, matched - skipped),
    dispatched: number(value.dispatched),
    skipped,
    failed: number(value.failed),
    results,
  }
}

export function mapPlacement(raw: unknown): CapabilityPlacement | null {
  const value = object(raw)
  if (value.capabilityId == null) return null
  return {
    id: string(value.id),
    capabilityId: string(value.capabilityId),
    label: string(value.label),
    x: number(value.x, 50),
    y: number(value.y, 50),
    width: number(value.width, 10),
    height: number(value.height, 10),
    sortOrder: number(value.sortOrder),
    config: jsonObject(value.config),
  }
}

export const mockFolders: FolderDto[] = [
  { id: 'site', parentId: null, name: 'Khu nhà thông minh', templateType: 'OUTDOOR', templateConfig: {}, sortOrder: 0, permissions: ['NODE_VIEW', 'NODE_CONTROL', 'AUDIT_VIEW'] },
  { id: 'building-a', parentId: 'site', name: 'Tòa nhà A', templateType: 'BUILDING', templateConfig: {}, sortOrder: 0, permissions: ['NODE_VIEW', 'NODE_CONTROL', 'AUDIT_VIEW'] },
  { id: 'floor-1', parentId: 'building-a', name: 'Tầng trệt', templateType: 'FLOOR', templateConfig: { rooms: 3 }, sortOrder: 0, permissions: ['NODE_VIEW', 'NODE_CONTROL', 'AUDIT_VIEW'] },
  { id: 'corridor-1', parentId: 'floor-1', name: 'Hành lang phía Đông', templateType: 'CORRIDOR', templateConfig: {}, sortOrder: 0, permissions: ['NODE_VIEW', 'NODE_CONTROL'] },
  { id: 'room-living', parentId: 'floor-1', name: 'Phòng khách', templateType: 'ROOM', templateConfig: {}, sortOrder: 1, permissions: ['NODE_VIEW', 'NODE_CONTROL'] },
  { id: 'room-bedroom', parentId: 'floor-1', name: 'Phòng ngủ', templateType: 'ROOM', templateConfig: {}, sortOrder: 2, permissions: ['NODE_VIEW'] },
]

const mockDeviceTypes: DeviceType[] = [{ id: 'lighting', name: 'Lighting', icon: 'i-lucide-lightbulb' }, { id: 'fan', name: 'Fan', icon: 'i-lucide-fan' }, { id: 'sensor', name: 'Sensor', icon: 'i-lucide-gauge' }]
const mockTags: Tag[] = [{ id: 'critical', name: 'Ưu tiên' }, { id: 'hvac', name: 'Thông gió' }]
export const mockPlacements: Record<string, CapabilityPlacement[]> = {
  'floor-1': [{ id: 'placement-c1', capabilityId: 'relay-c1', label: 'Quạt trần', x: 50, y: 75, width: 10, height: 10, sortOrder: 0, config: { rotation: 0, scale: 1 } }],
  'room-living': [
    { id: 'placement-a1', capabilityId: 'relay-a1', label: 'Đèn trần', x: 28, y: 54, width: 10, height: 10, sortOrder: 0, config: { rotation: 0, scale: 1 } },
    { id: 'placement-a2', capabilityId: 'relay-a2', label: 'Đèn bàn', x: 66, y: 66, width: 10, height: 10, sortOrder: 1, config: { rotation: 0, scale: 1 } },
  ],
  'room-bedroom': [{ id: 'placement-b1', capabilityId: 'relay-b1', label: 'Đèn ngủ', x: 70, y: 42, width: 10, height: 10, sortOrder: 0, config: { rotation: 0, scale: 1 } }],
}

export async function listFolders(): Promise<FolderDto[]> {
  if (isMock) return structuredClone(mockFolders)
  const raw = (await http.get<unknown>('/folders')).data
  return list(object(raw).items ?? object(raw).content ?? raw).map(mapFolder).filter((folder) => folder.id)
}

export async function getFolderMap(folderId: string): Promise<FolderMap> {
  if (isMock) {
    const folder = mockFolders.find((item) => item.id === folderId)
    if (!folder) throw new Error(`Folder ${folderId} not found`)
    return { folder: structuredClone(folder), nodes: structuredClone(mockNodes.filter((node) => node.folderId === folderId)), placements: structuredClone(mockPlacements[folderId] ?? []) }
  }
  const raw = (await http.get<unknown>(`/folders/${encodeURIComponent(folderId)}/map`)).data
  const value = object(raw)
  const folder = mapFolder(value.folder ?? value)
  const rawNodes = list(value.nodes ?? object(value.map).nodes)
  const placements = list(value.placements ?? object(value.map).placements).map(mapPlacement).filter((placement): placement is CapabilityPlacement => placement !== null)
  return { folder, nodes: rawNodes.map((node) => mapNodeDto(node as NodeDto)), placements }
}

export async function listDeviceTypes(): Promise<DeviceType[]> {
  if (isMock) return structuredClone(mockDeviceTypes)
  const raw = (await http.get<unknown>('/device-types')).data
  return list(object(raw).items ?? raw).map(mapCatalogItem).filter((item): item is DeviceType => item !== null)
}

export async function listTags(): Promise<Tag[]> {
  if (isMock) return structuredClone(mockTags)
  const raw = (await http.get<unknown>('/tags')).data
  return list(object(raw).items ?? raw).map(mapCatalogItem).filter((item): item is Tag => item !== null).map(({ id, name, color }) => ({ id, name, color }))
}

export async function previewBulkAction(folderId: string, payload: BulkActionPayload): Promise<BulkActionResult> {
  if (isMock) return mockBulk(folderId, payload, false)
  return mapBulk((await http.post<unknown>(`/folders/${encodeURIComponent(folderId)}/bulk-actions/preview`, bulkWirePayload(payload))).data)
}

export async function executeBulkAction(folderId: string, payload: BulkActionPayload): Promise<BulkActionResult> {
  if (isMock) return mockBulk(folderId, payload, true)
  return mapBulk((await http.post<unknown>(`/folders/${encodeURIComponent(folderId)}/bulk-actions`, bulkWirePayload(payload))).data)
}

function bulkWirePayload(payload: BulkActionPayload) {
  return {
    ...payload,
    deviceTypeIds: payload.deviceTypeIds.map(Number),
    tagIds: payload.tagIds.map(Number),
  }
}

function mockBulk(folderId: string, payload: BulkActionPayload, execute: boolean): BulkActionResult {
  const descendants = new Set([folderId])
  if (payload.includeDescendants) {
    let changed = true
    while (changed) {
      changed = false
      for (const folder of mockFolders) if (folder.parentId && descendants.has(folder.parentId) && !descendants.has(folder.id)) { descendants.add(folder.id); changed = true }
    }
  }
  const relays = mockNodes.filter((node) => descendants.has(node.folderId ?? '')).flatMap((node) => node.relays.map((relay) => ({ node, relay })))
    .filter(({ relay }) => !payload.deviceTypeIds.length || (relay.deviceType && payload.deviceTypeIds.includes(relay.deviceType.id)))
    .filter(({ relay }) => !payload.tagIds.length || (payload.tagMatch === 'ALL' ? payload.tagIds.every((id) => relay.tags.some((tag) => tag.id === id)) : payload.tagIds.some((id) => relay.tags.some((tag) => tag.id === id))))
  if (execute) for (const { node, relay } of relays) if (node.online) relay.state = payload.state
  const skipped = relays.filter(({ node }) => !node.online).length
  return { batchId: execute ? crypto.randomUUID() : null, matched: relays.length, dispatchable: relays.length - skipped, dispatched: execute ? relays.length - skipped : 0, skipped, failed: 0,
    results: relays.map(({ node, relay }) => ({ capabilityId: relay.id ?? '', nodeId: node.nodeId, channel: relay.channel, online: node.online, status: node.online ? (execute ? 'DISPATCHED' : 'DISPATCHABLE') : 'SKIPPED_OFFLINE' })) }
}

function auditResult(action: string): string {
  const suffix = action.split('_').at(-1) ?? ''
  if (action.includes('_SKIPPED_') || suffix === 'SKIPPED' || suffix === 'OFFLINE') return 'SKIPPED'
  if (suffix === 'REQUESTED') return 'PENDING'
  if (['FAILED', 'FAILURE', 'ERROR'].includes(suffix)) return 'FAILED'
  if (['REJECTED', 'DENIED'].includes(suffix)) return 'REJECTED'
  return 'SUCCESS'
}

function auditParams(filters: AuditFilters): Record<string, unknown> {
  const instant = (value?: string) => value ? new Date(value).toISOString() : undefined
  return {
    folderId: filters.folderId,
    includeDescendants: filters.folderId ? filters.includeDescendants : false,
    actor: filters.actor || undefined,
    node: filters.node || undefined,
    action: filters.action || undefined,
    from: instant(filters.from),
    to: instant(filters.to),
    page: filters.page,
    size: filters.size,
  }
}

export async function listAuditLogs(filters: AuditFilters): Promise<PageResult<AuditLog>> {
  if (isMock) {
    const all = Array.from({ length: 23 }, (_, index): AuditLog => { const action = index % 7 ? (index % 3 ? 'RELAY_COMMAND_DISPATCHED' : 'NODE_APPROVED') : 'RELAY_COMMAND_SKIPPED'; const nodeId = index % 3 ? mockNodes[index % mockNodes.length]?.nodeId ?? null : null; return { id: String(index + 1), timestamp: new Date(Date.now() - index * 420_000).toISOString(), action, actor: index % 2 ? 'admin' : 'operator', actorUserId: String(index % 2 ? 1 : 2), nodeId, folderId: 'floor-1', batchId: index < 4 ? 'batch-7f31' : null, correlationId: `corr-${1000 + index}`, result: auditResult(action), details: {}, targetType: nodeId ? 'NODE' : 'FOLDER', targetId: nodeId ?? 'floor-1', capabilityId: index % 3 ? 'relay-c1' : null, ip: '127.0.0.1' } })
    const filtered = all.filter((item) => (!filters.action || item.action.includes(filters.action)) && (!filters.actor || item.actor.includes(filters.actor)) && (!filters.node || item.nodeId?.includes(filters.node)) && (!filters.folderId || item.folderId === filters.folderId))
    const start = filters.page * filters.size
    return { items: filtered.slice(start, start + filters.size), page: filters.page, size: filters.size, total: filtered.length, totalPages: Math.ceil(filtered.length / filters.size) }
  }
  const raw = (await http.get<unknown>('/audit-logs', { params: auditParams(filters) })).data
  const value = object(raw)
  const items = list(value.items ?? value.content ?? value.results).map((rawItem): AuditLog => {
    const item = object(rawItem)
    const action = string(item.action)
    const details = jsonObject(item.details)
    const targetType = string(item.targetType)
    const targetId = item.targetId == null ? null : string(item.targetId)
    const actorUserId = item.actorUserId == null ? null : string(item.actorUserId)
    const nodeId = targetType === 'NODE' ? targetId : (details.nodeId == null ? null : string(details.nodeId))
    return { id: string(item.id), timestamp: string(item.createdAt), action, actor: string(item.actorName, actorUserId ? `#${actorUserId}` : 'SYSTEM'), actorUserId, nodeId, folderId: item.folderId == null ? null : string(item.folderId), batchId: item.batchId == null ? null : string(item.batchId), correlationId: item.correlationId == null ? null : string(item.correlationId), result: auditResult(action), details, targetType, targetId, capabilityId: item.capabilityId == null ? null : string(item.capabilityId), ip: item.ip == null ? null : string(item.ip) }
  })
  return { items, page: number(value.number, filters.page), size: number(value.size, filters.size), total: number(value.totalElements, items.length), totalPages: number(value.totalPages, 1) }
}
