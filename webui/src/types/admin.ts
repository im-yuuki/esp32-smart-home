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
  id: number
  name: string
  permissions: string[]
}

export interface AdminMember {
  userId: number
  username: string
  displayName: string
  roleId: number
  roleName: string
}

export interface AdminGroup {
  id: number
  name: string
  description: string | null
  roles: AdminRole[]
  members: AdminMember[]
  nodeIds: string[]
}

export interface AdminNode {
  nodeId: string
  room: string
  fwVersion: string | null
  ip: string | null
  online: boolean
  createdAt: string
  approvalStatus: 'PENDING' | 'APPROVED' | 'REJECTED'
  groupIds: number[]
}
