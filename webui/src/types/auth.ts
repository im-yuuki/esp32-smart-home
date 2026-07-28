export interface CsrfToken {
  token: string
  headerName: string
  /** XOR-masked token required by Spring Security's STOMP CONNECT interceptor. */
  stompToken?: string
}

export interface SessionUser {
  id: number
  username: string
  displayName: string
  systemAdmin: boolean
  mustChangePassword: boolean
  folders: Array<{ folderId: string; folderName: string; roleName?: string; permissions: string[] }>
  canViewAudit: boolean
  permissions: string[]
}
