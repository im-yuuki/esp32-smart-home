import './assets/main.css'

import { createApp } from 'vue'
import { createPinia } from 'pinia'

import App from './App.vue'
import router from './router'
import { setUnauthorizedHandler } from '@/api/http'
import { i18n } from '@/i18n'
import { useAuthStore } from '@/stores/auth'

const app = createApp(App)

const pinia = createPinia()
app.use(pinia)
setUnauthorizedHandler(() => {
  const auth = useAuthStore(pinia)
  if (!auth.isAuthenticated) return
  const path = router.currentRoute.value.fullPath
  auth.handleUnauthorized()
  const redirect = path.startsWith('/') && !path.startsWith('//') ? path : '/'
  void router.replace({ name: 'login', query: { redirect } })
})
app.use(router)
app.use(i18n)

app.mount('#app')
