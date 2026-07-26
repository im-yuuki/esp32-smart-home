/// <reference types="vite/client" />

interface ImportMetaEnv {
  /** Set to '1' to run against the built-in mock api + mock socket (no backend needed). */
  readonly VITE_MOCK?: string
}

interface ImportMeta {
  readonly env: ImportMetaEnv
}
