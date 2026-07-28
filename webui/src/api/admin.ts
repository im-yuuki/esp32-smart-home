import { http } from './http'
import { isMock } from './mock'
import { mockFolders, mockPlacements } from './facilities'
import { mockNodes } from './mock/fixtures'
import type { AdminFolder, AdminMember, AdminNode, AdminRole, AdminUser } from '@/types/admin'
import type { CapabilityMetadataMutation, CapabilityPlacementMutation, FolderMutation } from '@/types/facility'

const object = (value: unknown): Record<string, unknown> =>
  value && typeof value === 'object' && !Array.isArray(value) ? value as Record<string, unknown> : {}
const list = (value: unknown): unknown[] => Array.isArray(value) ? value : []
const string = (value: unknown, fallback = '') => value == null ? fallback : String(value)
const jsonObject = (value: unknown): Record<string, unknown> => {
  if (typeof value === 'string') {
    try { return object(JSON.parse(value)) } catch { return {} }
  }
  return object(value)
}
const longId = (value: string | null): number | null => value == null || value === '' ? null : Number(value)
const jsonClone = <T>(value: T): T => JSON.parse(JSON.stringify(value)) as T

const mockUsers: AdminUser[] = [{
  id: 1, username: 'admin', displayName: 'Mock Administrator', enabled: true,
  systemAdmin: true, mustChangePassword: false, createdAt: new Date().toISOString(),
}]
const mockRoles = new Map<string, AdminRole[]>(mockFolders.map((folder) => [folder.id, [
  { id: `${folder.id}-operator`, name: 'Operator', permissions: ['NODE_VIEW', 'NODE_CONTROL', 'TELEMETRY_VIEW'] },
  { id: `${folder.id}-viewer`, name: 'Viewer', permissions: ['NODE_VIEW', 'TELEMETRY_VIEW'] },
]]))
const mockMembers = new Map<string, AdminMember[]>(mockFolders.map((folder) => [folder.id, []]))

function mapRole(raw: unknown): AdminRole | null {
  const value = object(raw)
  if (value.id == null) return null
  return { id: string(value.id), name: string(value.name), permissions: list(value.permissions).filter((item): item is string => typeof item === 'string') }
}

function mapMember(raw: unknown): AdminMember | null {
  const value = object(raw)
  if (value.userId == null || value.roleId == null) return null
  return { userId: Number(value.userId), username: string(value.username), displayName: string(value.displayName, string(value.username)), roleId: string(value.roleId), roleName: string(value.roleName) }
}

export function mapAdminFolder(raw: unknown): AdminFolder {
  const value = object(raw)
  const template = string(value.templateType, 'ROOM').toUpperCase()
  return {
    id: string(value.id),
    parentId: value.parentId == null ? null : string(value.parentId),
    name: string(value.name, string(value.id)),
    templateType: ['OUTDOOR', 'BUILDING', 'FLOOR', 'CORRIDOR', 'ROOM'].includes(template) ? template as AdminFolder['templateType'] : 'ROOM',
    templateConfig: jsonObject(value.templateConfig),
    sortOrder: Number(value.sortOrder ?? 0),
    roles: list(value.roles).map(mapRole).filter((item): item is AdminRole => item !== null),
    members: list(value.members).map(mapMember).filter((item): item is AdminMember => item !== null),
    nodeIds: list(value.nodeIds).map(String),
  }
}

function mockAdminFolder(id: string): AdminFolder {
  const folder = mockFolders.find((item) => item.id === id)
  if (!folder) throw new Error(`Folder ${id} not found`)
  return { ...structuredClone(folder), roles: structuredClone(mockRoles.get(id) ?? []), members: structuredClone(mockMembers.get(id) ?? []), nodeIds: mockNodes.filter((node) => node.folderId === id).map((node) => node.nodeId) }
}

export async function listUsers(): Promise<AdminUser[]> {
  if (isMock) return structuredClone(mockUsers)
  return (await http.get<AdminUser[]>('/admin/users')).data
}

export async function createUser(payload: { username: string; displayName: string; temporaryPassword: string; systemAdmin: boolean }): Promise<void> {
  if (isMock) return
  await http.post('/admin/users', payload)
}

export async function setUserEnabled(userId: number, enabled: boolean): Promise<void> {
  if (isMock) return
  await http.patch(`/admin/users/${userId}/enabled`, { enabled })
}

export async function listPermissions(): Promise<string[]> {
  if (isMock) return ['NODE_VIEW', 'NODE_CONTROL', 'TELEMETRY_VIEW', 'AUDIT_VIEW']
  const response = await http.get<Array<{ code: string }>>('/admin/permissions')
  return response.data.map((item) => item.code)
}

export async function listAdminFolders(): Promise<AdminFolder[]> {
  if (isMock) return mockFolders.map((folder) => mockAdminFolder(folder.id))
  return (await http.get<unknown[]>('/admin/folders')).data.map(mapAdminFolder)
}

function folderMetadata(payload: FolderMutation) {
  return { parentId: longId(payload.parentId), name: payload.name, sortOrder: payload.sortOrder }
}

function folderTemplate(payload: FolderMutation) {
  return { templateType: payload.templateType, templateConfig: JSON.stringify(payload.templateConfig) }
}

export async function createFolder(payload: FolderMutation): Promise<AdminFolder> {
  if (isMock) {
    const id = `folder-${Date.now()}`
    mockFolders.push({ id, permissions: [], ...jsonClone(payload) })
    mockRoles.set(id, []); mockMembers.set(id, [])
    return mockAdminFolder(id)
  }
  const created = mapAdminFolder((await http.post<unknown>('/admin/folders', folderMetadata(payload))).data)
  return mapAdminFolder((await http.put<unknown>(`/admin/folders/${encodeURIComponent(created.id)}/template`, folderTemplate(payload))).data)
}

export async function updateFolder(id: string, payload: FolderMutation): Promise<AdminFolder> {
  if (isMock) {
    Object.assign(mockFolders.find((item) => item.id === id) ?? {}, jsonClone(payload))
    return mockAdminFolder(id)
  }
  await http.put(`/admin/folders/${encodeURIComponent(id)}`, folderMetadata(payload))
  return mapAdminFolder((await http.put<unknown>(`/admin/folders/${encodeURIComponent(id)}/template`, folderTemplate(payload))).data)
}

export async function moveFolder(id: string, parentId: string | null): Promise<void> {
  if (isMock) { const folder = mockFolders.find((item) => item.id === id); if (folder) folder.parentId = parentId; return }
  await http.post(`/admin/folders/${encodeURIComponent(id)}/move`, { parentId: longId(parentId) })
}

export async function deleteFolder(id: string): Promise<void> {
  if (isMock) { const index = mockFolders.findIndex((item) => item.id === id); if (index >= 0) mockFolders.splice(index, 1); mockRoles.delete(id); mockMembers.delete(id); return }
  await http.delete(`/admin/folders/${encodeURIComponent(id)}`)
}

export async function createFolderRole(folderId: string, payload: { name: string; permissions: string[] }): Promise<void> {
  if (isMock) { const roles = mockRoles.get(folderId) ?? []; roles.push({ id: `role-${Date.now()}`, ...payload }); mockRoles.set(folderId, roles); return }
  await http.post(`/admin/folders/${encodeURIComponent(folderId)}/roles`, payload)
}

export async function assignFolderRole(folderId: string, userId: number, roleId: string): Promise<void> {
  if (isMock) {
    const role = mockRoles.get(folderId)?.find((item) => item.id === roleId)
    const user = mockUsers.find((item) => item.id === userId)
    if (role && user) mockMembers.set(folderId, [{ userId, username: user.username, displayName: user.displayName, roleId, roleName: role.name }])
    return
  }
  await http.put(`/admin/folders/${encodeURIComponent(folderId)}/members/${userId}`, { roleId: Number(roleId) })
}

export async function removeFolderMember(folderId: string, userId: number): Promise<void> {
  if (isMock) { mockMembers.set(folderId, (mockMembers.get(folderId) ?? []).filter((item) => item.userId !== userId)); return }
  await http.delete(`/admin/folders/${encodeURIComponent(folderId)}/members/${userId}`)
}

function mapAdminNode(raw: unknown): AdminNode {
  const value = object(raw)
  const status = string(value.approvalStatus, 'PENDING')
  return {
    nodeId: string(value.nodeId), discoveryName: string(value.discoveryName), displayName: value.displayName == null ? null : string(value.displayName), room: string(value.room), folderId: value.folderId == null ? null : string(value.folderId), fwVersion: value.fwVersion == null ? null : string(value.fwVersion), ip: value.ip == null ? null : string(value.ip), online: value.online === true, createdAt: string(value.createdAt), approvalStatus: ['PENDING', 'APPROVED', 'REJECTED'].includes(status) ? status as AdminNode['approvalStatus'] : 'PENDING',
  }
}

export async function listAdminNodes(status: AdminNode['approvalStatus']): Promise<AdminNode[]> {
  if (isMock) return status === 'PENDING' ? [{ nodeId: 'esp32s3-new001', discoveryName: 'ESP32 mới', displayName: null, room: 'unassigned', folderId: null, fwVersion: '1.1.0', ip: '192.168.1.70', online: true, createdAt: new Date().toISOString(), approvalStatus: 'PENDING' }] : mockNodes.map((node) => ({ nodeId: node.nodeId, discoveryName: node.discoveryName ?? '', displayName: node.displayName, room: node.room, folderId: node.folderId, fwVersion: node.fwVersion ?? null, ip: node.ip ?? null, online: node.online, createdAt: new Date().toISOString(), approvalStatus: 'APPROVED' }))
  return (await http.get<unknown[]>('/admin/nodes', { params: { status } })).data.map(mapAdminNode)
}

export async function approveNodeToFolder(nodeId: string, folderId: string): Promise<void> {
  if (isMock) return
  await http.post(`/admin/nodes/${encodeURIComponent(nodeId)}/approve`, { folderId: Number(folderId) })
}

export async function rejectNode(nodeId: string): Promise<void> {
  if (isMock) return
  await http.post(`/admin/nodes/${encodeURIComponent(nodeId)}/reject`)
}

export async function setNodeFolder(nodeId: string, folderId: string): Promise<void> {
  if (isMock) { const node = mockNodes.find((item) => item.nodeId === nodeId); if (node) node.folderId = folderId; return }
  await http.put(`/admin/nodes/${encodeURIComponent(nodeId)}/folder`, { folderId: Number(folderId) })
}

export async function patchNodeDisplayName(nodeId: string, displayName: string | null): Promise<void> {
  if (isMock) { const node = mockNodes.find((item) => item.nodeId === nodeId); if (node) node.displayName = displayName; return }
  await http.patch(`/admin/nodes/${encodeURIComponent(nodeId)}/display-name`, { displayName })
}

export async function patchCapabilityMetadata(capabilityId: string, payload: CapabilityMetadataMutation): Promise<void> {
  if (isMock) {
    const relay = mockNodes.flatMap((node) => node.relays).find((item) => item.id === capabilityId)
    if (relay) { relay.displayName = payload.displayName; relay.deviceType = payload.deviceTypeId ? { id: payload.deviceTypeId, name: payload.deviceTypeId } : null; relay.tags = payload.tagIds.map((id) => ({ id, name: id })) }
    return
  }
  await http.patch(`/admin/capabilities/${encodeURIComponent(capabilityId)}`, { displayName: payload.displayName, deviceTypeId: payload.deviceTypeId == null ? null : Number(payload.deviceTypeId), tagIds: payload.tagIds.map(Number) })
}

export async function updatePlacement(folderId: string, capabilityId: string, payload: CapabilityPlacementMutation): Promise<void> {
  if (isMock) {
    const placements = mockPlacements[folderId] ?? []
    const current = placements.find((item) => item.capabilityId === capabilityId)
    if (current) Object.assign(current, jsonClone(payload))
    else placements.push({ id: `placement-${Date.now()}`, capabilityId, label: '', ...jsonClone(payload) })
    mockPlacements[folderId] = placements
    return
  }
  await http.put(`/admin/folders/${encodeURIComponent(folderId)}/placements/${encodeURIComponent(capabilityId)}`, { ...payload, config: JSON.stringify(payload.config) })
}

export async function createDeviceType(name: string, description: string): Promise<void> {
  if (isMock) return
  await http.post('/admin/device-types', { name, description })
}

export async function createTag(name: string, color: string | null): Promise<void> {
  if (isMock) return
  await http.post('/admin/tags', { name, color })
}
