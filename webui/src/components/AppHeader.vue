<script setup lang="ts">
import ConnectionIndicator from '@/components/ConnectionIndicator.vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const auth = useAuthStore()
const router = useRouter()

async function logout(): Promise<void> {
  try {
    await auth.logout()
  } finally {
    await router.replace({ name: 'login' })
  }
}
</script>

<template>
  <header class="sticky top-0 z-40 border-b border-default bg-default/80 backdrop-blur">
    <UContainer class="flex h-14 items-center justify-between">
      <RouterLink to="/" class="flex items-center gap-2 font-semibold text-highlighted">
        <UIcon name="i-lucide-house" class="size-5 text-primary" />
        <span>Smart Home</span>
      </RouterLink>
      <div v-if="auth.isAuthenticated" class="flex items-center gap-2">
        <ConnectionIndicator />
        <UButton v-if="auth.user?.systemAdmin" to="/admin" color="neutral" variant="ghost" icon="i-lucide-shield">Admin</UButton>
        <span class="hidden text-sm text-muted sm:inline">{{ auth.user?.displayName }}</span>
        <UButton color="neutral" variant="ghost" icon="i-lucide-log-out" aria-label="Sign out" @click="logout" />
      </div>
    </UContainer>
  </header>
</template>
