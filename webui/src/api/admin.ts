import { http } from './http'
import { isMock } from './mock'
import type { AdminGroup, AdminNode, AdminRole, AdminUser } from '@/types/admin'

const mockUsers: AdminUser[] = [{
  id: 1, username: 'admin', displayName: 'Mock Administrator', enabled: true,
  systemAdmin: true, mustChangePassword: false, createdAt: new Date().toISOString(),
}]
const mockGroups: AdminGroup[] = [
  { id: 1, name: 'Ground floor', description: null, nodeIds: ['esp32s3-aabbcc', 'esp32s3-badbad'], members: [], roles: [
    { id: 1, name: 'Operator', permissions: ['NODE_VIEW', 'NODE_CONTROL', 'TELEMETRY_VIEW'] },
  ] },
  { id: 2, name: 'Bedrooms', description: null, nodeIds: ['esp32s3-c0ffee', 'esp32s3-badbad'], members: [], roles: [
    { id: 2, name: 'Viewer', permissions: ['NODE_VIEW', 'TELEMETRY_VIEW'] },
  ] },
]

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
  if (isMock) return ['NODE_VIEW', 'NODE_CONTROL', 'TELEMETRY_VIEW']
  const response = await http.get<Array<{ code: string }>>('/admin/permissions')
  return response.data.map((item) => item.code)
}

export async function listGroups(): Promise<AdminGroup[]> {
  if (isMock) return structuredClone(mockGroups)
  return (await http.get<AdminGroup[]>('/admin/groups')).data
}

export async function createGroup(payload: { name: string; description: string }): Promise<void> {
  if (isMock) return
  await http.post('/admin/groups', payload)
}

export async function createRole(groupId: number, payload: { name: string; permissions: string[] }): Promise<AdminRole | null> {
  if (isMock) return null
  return (await http.post<AdminRole>(`/admin/groups/${groupId}/roles`, payload)).data
}

export async function assignRole(groupId: number, userId: number, roleId: number): Promise<void> {
  if (isMock) return
  await http.put(`/admin/groups/${groupId}/members/${userId}`, { roleId })
}

export async function removeMember(groupId: number, userId: number): Promise<void> {
  if (isMock) return
  await http.delete(`/admin/groups/${groupId}/members/${userId}`)
}

export async function listAdminNodes(status: AdminNode['approvalStatus']): Promise<AdminNode[]> {
  if (isMock) return []
  return (await http.get<AdminNode[]>('/admin/nodes', { params: { status } })).data
}

export async function approveNode(nodeId: string, groupIds: number[]): Promise<void> {
  if (isMock) return
  await http.post(`/admin/nodes/${encodeURIComponent(nodeId)}/approve`, { groupIds })
}

export async function rejectNode(nodeId: string): Promise<void> {
  if (isMock) return
  await http.post(`/admin/nodes/${encodeURIComponent(nodeId)}/reject`)
}

export async function setNodeGroups(nodeId: string, groupIds: number[]): Promise<void> {
  if (isMock) return
  await http.put(`/admin/nodes/${encodeURIComponent(nodeId)}/groups`, { groupIds })
}
