import { createRouter, createWebHistory } from 'vue-router'
import DashboardView from '@/views/DashboardView.vue'

const router = createRouter({
  // History mode: the serving layer must fall back to index.html
  // (Vite dev server does; prod nginx uses `try_files ... /index.html`).
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    { path: '/', name: 'dashboard', component: DashboardView },
    {
      path: '/nodes/:nodeId',
      name: 'node-detail',
      component: () => import('@/views/NodeDetailView.vue'),
    },
  ],
})

export default router
