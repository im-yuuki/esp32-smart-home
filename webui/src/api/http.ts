import axios, { AxiosError } from 'axios'

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
  headers: { Accept: 'application/json' },
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
    if (isEnvelope(body) && body.error) {
      throw new ApiError(body.error.code, body.error.message, error.response?.status)
    }
    throw new ApiError('NETWORK', error.message || 'Network error', error.response?.status)
  },
)
