import { createRouter, createWebHistory } from 'vue-router'
import DashboardView from '@/views/DashboardView.vue'
import { useAuthStore } from '@/stores/auth'

const router = createRouter({
  // History mode: the serving layer must fall back to index.html
  // (Vite dev server does; prod nginx uses `try_files ... /index.html`).
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    { path: '/login', name: 'login', component: () => import('@/views/LoginView.vue'), meta: { guestOnly: true } },
    { path: '/change-password', name: 'change-password', component: () => import('@/views/ChangePasswordView.vue'), meta: { requiresAuth: true } },
    { path: '/', name: 'dashboard', component: DashboardView, meta: { requiresAuth: true } },
    { path: '/browse/:folderId', name: 'browse', component: () => import('@/views/FolderView.vue'), meta: { requiresAuth: true } },
    { path: '/logs', name: 'logs', component: () => import('@/views/LogsView.vue'), meta: { requiresAuth: true, requiresAudit: true } },
    {
      path: '/nodes/:nodeId',
      name: 'node-detail',
      component: () => import('@/views/NodeDetailView.vue'),
      meta: { requiresAuth: true },
    },
    { path: '/admin', name: 'admin', component: () => import('@/views/AdminView.vue'), meta: { requiresAuth: true, requiresAdmin: true } },
  ],
})

router.beforeEach(async (to) => {
  const auth = useAuthStore()
  await auth.initialize()
  if (to.meta.requiresAuth && !auth.isAuthenticated) {
    const redirect = to.fullPath.startsWith('/') && !to.fullPath.startsWith('//') ? to.fullPath : '/'
    return { name: 'login', query: { redirect } }
  }
  if (auth.isAuthenticated && auth.user?.mustChangePassword && to.name !== 'change-password') {
    return { name: 'change-password' }
  }
  if (to.meta.requiresAdmin && !auth.user?.systemAdmin) return { name: 'dashboard' }
  if (to.meta.requiresAudit && !auth.user?.systemAdmin && !auth.user?.canViewAudit) return { name: 'dashboard' }
  if (to.meta.guestOnly && auth.isAuthenticated) return { name: 'dashboard' }
  return true
})

export default router
