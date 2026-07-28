export interface AdminUser {
  id: number
  username: string
  displayName: string
  enabled: boolean
  systemAdmin: boolean
  mustChangePassword: boolean
  createdAt: string
}

export interface AdminRole {
  id: string
  name: string
  permissions: string[]
}

export interface AdminMember {
  userId: number
  username: string
  displayName: string
  roleId: string
  roleName: string
}

export interface AdminFolder {
  id: string
  parentId: string | null
  name: string
  templateType: import('./facility').FolderTemplate
  templateConfig: Record<string, unknown>
  sortOrder: number
  roles: AdminRole[]
  members: AdminMember[]
  nodeIds: string[]
}

export interface AdminNode {
  nodeId: string
  discoveryName: string
  displayName?: string | null
  room: string
  folderId?: string | null
  fwVersion: string | null
  ip: string | null
  online: boolean
  createdAt: string
  approvalStatus: 'PENDING' | 'APPROVED' | 'REJECTED'
}
