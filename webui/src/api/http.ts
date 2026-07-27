import axios, { AxiosError, type InternalAxiosRequestConfig } from 'axios'

/** Backend response envelope: every /api/v1 response is `{ data, error }`. */
export interface ApiErrorBody {
  code: string
  message: string
}

export interface ApiEnvelope<T> {
  data: T
  error: ApiErrorBody | null
}

export class ApiError extends Error {
  constructor(
    readonly code: string,
    message: string,
    readonly status?: number,
  ) {
    super(message)
    this.name = 'ApiError'
  }
}

function isEnvelope(body: unknown): body is ApiEnvelope<unknown> {
  return typeof body === 'object' && body !== null && 'data' in body && 'error' in body
}

/**
 * Shared axios instance. All URLs are relative (`/api/v1/...`) so the Vite dev
 * proxy and the prod same-origin nginx both work unchanged.
 *
 * The response interceptor unwraps the ApiResponse envelope: callers receive
 * the inner `data` directly, and envelope errors reject as `ApiError`.
 */
export const http = axios.create({
  baseURL: '/api/v1',
  timeout: 10_000,
  withCredentials: true,
  headers: { Accept: 'application/json' },
})

let csrfToken: { token: string; headerName: string } | null = null
let unauthorizedHandler: (() => void) | null = null
let authGeneration = 0

type GeneratedRequestConfig = InternalAxiosRequestConfig & { authGeneration?: number }

export function advanceAuthGeneration(): void {
  authGeneration++
}

export function setCsrfToken(value: { token: string; headerName: string } | null): void {
  csrfToken = value
}

export function getCsrfToken(): { token: string; headerName: string } | null {
  return csrfToken
}

export function setUnauthorizedHandler(handler: () => void): void {
  unauthorizedHandler = handler
}

http.interceptors.request.use((config) => {
  ;(config as GeneratedRequestConfig).authGeneration = authGeneration
  if (csrfToken) config.headers.set(csrfToken.headerName, csrfToken.token)
  return config
})

http.interceptors.response.use(
  (response) => {
    const body: unknown = response.data
    if (isEnvelope(body)) {
      if (body.error) {
        throw new ApiError(body.error.code, body.error.message, response.status)
      }
      response.data = body.data
    }
    return response
  },
  (error: AxiosError<unknown>) => {
    const body = error.response?.data
    const url = error.config?.url ?? ''
    const requestGeneration = (error.config as GeneratedRequestConfig | undefined)?.authGeneration
    if (error.response?.status === 401 && requestGeneration === authGeneration
        && !url.endsWith('/auth/me') && !url.endsWith('/auth/login')) {
      unauthorizedHandler?.()
    }
    if (isEnvelope(body) && body.error) {
      throw new ApiError(body.error.code, body.error.message, error.response?.status)
    }
    throw new ApiError('NETWORK', error.message || 'Network error', error.response?.status)
  },
)
