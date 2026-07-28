import { createI18n } from 'vue-i18n'
import en from './locales/en'
import vi from './locales/vi'

export type AppLocale = 'en' | 'vi'

const STORAGE_KEY = 'smarthome.locale'
const supportedLocales: AppLocale[] = ['en', 'vi']

function isAppLocale(value: string | null): value is AppLocale {
  return value !== null && supportedLocales.includes(value as AppLocale)
}

function initialLocale(): AppLocale {
  const stored = window.localStorage.getItem(STORAGE_KEY)
  if (isAppLocale(stored)) return stored
  return window.navigator.language.toLowerCase().startsWith('vi') ? 'vi' : 'en'
}

export const i18n = createI18n({
  legacy: false,
  locale: initialLocale(),
  fallbackLocale: 'en',
  messages: { en, vi },
  numberFormats: {
    en: {
      oneDecimal: { minimumFractionDigits: 1, maximumFractionDigits: 1 },
      integer: { maximumFractionDigits: 0 },
    },
    vi: {
      oneDecimal: { minimumFractionDigits: 1, maximumFractionDigits: 1 },
      integer: { maximumFractionDigits: 0 },
    },
  },
})

export function setLocale(locale: AppLocale): void {
  i18n.global.locale.value = locale
  window.localStorage.setItem(STORAGE_KEY, locale)
  document.documentElement.lang = locale
}

setLocale(i18n.global.locale.value as AppLocale)
