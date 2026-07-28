import type { DeviceType, NodeInfo, Tag } from './api'

export type FolderTemplate = 'OUTDOOR' | 'BUILDING' | 'FLOOR' | 'CORRIDOR' | 'ROOM'

export interface FolderDto {
  id: string
  parentId: string | null
  name: string
  templateType: FolderTemplate
  templateConfig: Record<string, unknown>
  sortOrder: number
  permissions: string[]
}

export interface FolderMap {
  folder: FolderDto
  nodes: NodeInfo[]
  placements: CapabilityPlacement[]
}

export interface CapabilityPlacement {
  id: string
  capabilityId: string
  label: string
  x: number
  y: number
  width: number
  height: number
  sortOrder: number
  config: Record<string, unknown>
}

export interface BulkActionPayload {
  includeDescendants: boolean
  deviceTypeIds: string[]
  tagIds: string[]
  tagMatch: 'ANY' | 'ALL'
  state: 'ON' | 'OFF'
  idempotencyKey: string
}

export interface BulkActionResult {
  batchId: string | null
  matched: number
  dispatchable: number
  dispatched: number
  skipped: number
  failed: number
  results: Array<{ capabilityId: string; nodeId: string; channel: number; online: boolean; status: string }>
}

export interface AuditFilters {
  from?: string
  to?: string
  folderId?: string
  action?: string
  actor?: string
  node?: string
  includeDescendants: boolean
  page: number
  size: number
}

export interface AuditLog {
  id: string
  timestamp: string
  action: string
  actor: string
  actorUserId: string | null
  nodeId: string | null
  folderId: string | null
  batchId: string | null
  correlationId: string | null
  result: string
  details: Record<string, unknown>
  targetType: string
  targetId: string | null
  capabilityId: string | null
  ip: string | null
}

export interface PageResult<T> {
  items: T[]
  page: number
  size: number
  total: number
  totalPages: number
}

export interface FolderRole {
  id: string
  name: string
  permissions: string[]
}

export interface FolderMembership {
  userId: number
  username: string
  displayName: string
  roleId: string
  roleName: string
}

export interface FolderMutation {
  parentId: string | null
  name: string
  templateType: FolderTemplate
  templateConfig: Record<string, unknown>
  sortOrder: number
}

export interface CapabilityMetadataMutation {
  displayName: string | null
  deviceTypeId: string | null
  tagIds: string[]
}

export interface CapabilityPlacementMutation {
  x: number
  y: number
  width: number
  height: number
  sortOrder: number
  config: Record<string, unknown>
}

export type { DeviceType, Tag }
