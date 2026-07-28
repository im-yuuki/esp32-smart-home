import { ApiError } from '@/api/http'
import { i18n } from '@/i18n'

export function localizedError(error: unknown): string {
  if (error instanceof ApiError) {
    const key = `errors.${error.code}`
    if (i18n.global.te(key)) return i18n.global.t(key)
    return error.message
  }
  if (error instanceof Error) return error.message
  return i18n.global.t('errors.unknown')
}
