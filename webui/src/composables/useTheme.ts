import { computed, ref } from 'vue'

export type ThemeMode = 'light' | 'dark'

const STORAGE_KEY = 'smarthome.theme'
const mode = ref<ThemeMode>('light')

function preferred(): ThemeMode {
  const stored = window.localStorage.getItem(STORAGE_KEY)
  if (stored === 'dark' || stored === 'light') return stored
  return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'
}

export function applyTheme(value: ThemeMode): void {
  mode.value = value
  document.documentElement.classList.toggle('app-dark', value === 'dark')
  document.documentElement.style.colorScheme = value
  window.localStorage.setItem(STORAGE_KEY, value)
}

export function useTheme() {
  function toggle(): void { applyTheme(mode.value === 'dark' ? 'light' : 'dark') }
  return { mode: computed(() => mode.value), toggle }
}

applyTheme(preferred())
