import { ApiError, http, setCsrfToken } from './http'
import { isMock } from './mock'
import type { CsrfToken, SessionUser } from '@/types/auth'

let mockAuthenticated = true

const mockUser: SessionUser = {
  id: 1,
  username: 'admin',
  displayName: 'Mock Administrator',
  systemAdmin: true,
  mustChangePassword: false,
  groups: [
    { id: 1, name: 'Ground floor', roleName: 'Operator' },
    { id: 2, name: 'Bedrooms', roleName: 'Viewer' },
  ],
}

export async function refreshCsrf(): Promise<CsrfToken> {
  if (isMock) {
    const token = { token: 'mock-csrf', headerName: 'X-CSRF-TOKEN' }
    setCsrfToken(token)
    return token
  }
  const response = await http.get<CsrfToken>('/auth/csrf')
  setCsrfToken(response.data)
  return response.data
}

export async function currentSession(): Promise<SessionUser> {
  if (isMock) {
    if (!mockAuthenticated) throw new ApiError('UNAUTHORIZED', 'Not authenticated', 401)
    return structuredClone(mockUser)
  }
  const response = await http.get<SessionUser>('/auth/me')
  return response.data
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
