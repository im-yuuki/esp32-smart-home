import { fileURLToPath, URL } from 'node:url'

import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import ui from '@nuxt/ui/vite'

// https://vite.dev/config/
export default defineConfig({
  plugins: [vue(), ui()],

  // sockjs-client references the Node-style `global` at import time
  define: { global: 'window' },

  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },

  server: {
    proxy: {
      '/api': { target: 'http://localhost:8080', changeOrigin: true },
      // ws:true covers the upgraded transport; SockJS's /ws/info + xhr fallbacks
      // are plain HTTP on the same prefix and are proxied too.
      '/ws': { target: 'http://localhost:8080', ws: true, changeOrigin: true },
    },
  },
})
