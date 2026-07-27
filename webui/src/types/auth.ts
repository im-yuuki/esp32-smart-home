export interface CsrfToken {
  token: string
  headerName: string
}

export interface UserGroup {
  id: number
  name: string
  roleName: string
}

export interface SessionUser {
  id: number
  username: string
  displayName: string
  systemAdmin: boolean
  mustChangePassword: boolean
  groups: UserGroup[]
}
