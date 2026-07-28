import { ApiError, http, setCsrfToken } from './http'
import { isMock } from './mock'
import type { CsrfToken, SessionUser } from '@/types/auth'

let mockAuthenticated = true

function csrfCookieToken(): string | null {
  if (typeof document === 'undefined') return null
  const prefix = 'XSRF-TOKEN='
  const cookie = document.cookie.split('; ').find((item) => item.startsWith(prefix))
  return cookie ? decodeURIComponent(cookie.slice(prefix.length)) : null
}

const mockUser: SessionUser = {
  id: 1,
  username: 'admin',
  displayName: 'Mock Administrator',
  systemAdmin: true,
  mustChangePassword: false,
  folders: [
    { folderId: 'floor-1', folderName: 'Tầng trệt', roleName: 'Operator', permissions: ['NODE_VIEW', 'NODE_CONTROL', 'TELEMETRY_VIEW', 'AUDIT_VIEW'] },
  ],
  canViewAudit: true,
  permissions: ['NODE_VIEW', 'NODE_CONTROL', 'TELEMETRY_VIEW', 'AUDIT_VIEW'],
}

function mapSession(raw: unknown): SessionUser {
  const value = (raw && typeof raw === 'object' ? raw : {}) as Record<string, unknown>
  const folders = Array.isArray(value.folders) ? value.folders : []
  const permissions = Array.isArray(value.permissions) ? value.permissions.filter((item): item is string => typeof item === 'string') : []
  return {
    id: Number(value.id ?? 0),
    username: String(value.username ?? ''),
    displayName: String(value.displayName ?? value.username ?? ''),
    systemAdmin: value.systemAdmin === true,
    mustChangePassword: value.mustChangePassword === true,
    folders: folders.flatMap((rawFolder) => {
      if (!rawFolder || typeof rawFolder !== 'object') return []
      const folder = rawFolder as Record<string, unknown>
      const folderId = folder.folderId ?? folder.id
      if (folderId == null) return []
      return [{
        folderId: String(folderId),
        folderName: String(folder.folderName ?? folder.name ?? folderId),
        roleName: typeof folder.roleName === 'string' ? folder.roleName : undefined,
        permissions: Array.isArray(folder.permissions) ? folder.permissions.filter((item): item is string => typeof item === 'string') : [],
      }]
    }),
    permissions,
    canViewAudit: value.auditAccess === true || value.canViewAudit === true || permissions.includes('AUDIT_VIEW') || value.systemAdmin === true,
  }
}

export async function refreshCsrf(): Promise<CsrfToken> {
  if (isMock) {
    const token = { token: 'mock-csrf', headerName: 'X-CSRF-TOKEN' }
    setCsrfToken(token)
    return token
  }
  const response = await http.get<CsrfToken>('/auth/csrf')
  // Spring Security's SPA handler exposes an XOR-masked request attribute in
  // the JSON response, while state-changing requests expect the raw token from
  // its same-origin XSRF-TOKEN cookie.
  const token = { ...response.data, token: csrfCookieToken() ?? response.data.token }
  setCsrfToken(token)
  return token
}

export async function currentSession(): Promise<SessionUser> {
  if (isMock) {
    if (!mockAuthenticated) throw new ApiError('UNAUTHORIZED', 'Not authenticated', 401)
    return structuredClone(mockUser)
  }
  const response = await http.get<SessionUser>('/auth/me')
  return mapSession(response.data)
}

export async function login(username: string, password: string): Promise<SessionUser> {
  if (isMock) {
    mockAuthenticated = true
    await refreshCsrf()
    return structuredClone(mockUser)
  }
  await refreshCsrf()
  await http.post('/auth/login', { username, password }, { headers: { 'Content-Type': 'application/json' } })
  await refreshCsrf()
  return currentSession()
}

export async function logout(): Promise<void> {
  if (isMock) {
    mockAuthenticated = false
    setCsrfToken(null)
    return
  }
  try {
    await http.post('/auth/logout')
  } finally {
    setCsrfToken(null)
  }
}

export async function changePassword(currentPassword: string, newPassword: string): Promise<void> {
  if (isMock) {
    if (newPassword.length < 12) throw new ApiError('VALIDATION_ERROR', 'Password must be at least 12 characters', 400)
    mockAuthenticated = false
    setCsrfToken(null)
    return
  }
  await http.post('/auth/change-password', { currentPassword, newPassword })
  setCsrfToken(null)
}
