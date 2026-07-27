import { computed, ref } from 'vue'
import { defineStore } from 'pinia'
import * as authApi from '@/api/auth'
import { advanceAuthGeneration, ApiError, setCsrfToken } from '@/api/http'
import type { SessionUser } from '@/types/auth'

export type AuthStatus = 'unknown' | 'loading' | 'authenticated' | 'anonymous'

export const useAuthStore = defineStore('auth', () => {
  const status = ref<AuthStatus>('unknown')
  const user = ref<SessionUser | null>(null)
  let initializeInFlight: Promise<void> | null = null

  const isAuthenticated = computed(() => status.value === 'authenticated' && user.value !== null)

  function setAnonymous(): void {
    advanceAuthGeneration()
    user.value = null
    status.value = 'anonymous'
    setCsrfToken(null)
  }

  function initialize(): Promise<void> {
    if (status.value === 'authenticated' || status.value === 'anonymous') return Promise.resolve()
    if (initializeInFlight) return initializeInFlight
    status.value = 'loading'
    initializeInFlight = (async () => {
      try {
        user.value = await authApi.currentSession()
        await authApi.refreshCsrf()
        status.value = 'authenticated'
      } catch (error) {
        if (error instanceof ApiError && error.status === 401) setAnonymous()
        else {
          status.value = 'unknown'
          throw error
        }
      } finally {
        initializeInFlight = null
      }
    })()
    return initializeInFlight
  }

  async function login(username: string, password: string): Promise<void> {
    advanceAuthGeneration()
    status.value = 'loading'
    try {
      user.value = await authApi.login(username, password)
      advanceAuthGeneration()
      status.value = 'authenticated'
    } catch (error) {
      setAnonymous()
      throw error
    }
  }

  async function logout(): Promise<void> {
    try {
      await authApi.logout()
    } finally {
      setAnonymous()
    }
  }

  async function changePassword(currentPassword: string, newPassword: string): Promise<void> {
    await authApi.changePassword(currentPassword, newPassword)
    setAnonymous()
  }

  return {
    status,
    user,
    isAuthenticated,
    initialize,
    login,
    logout,
    changePassword,
    handleUnauthorized: setAnonymous,
  }
})
