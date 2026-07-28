<script setup lang="ts">
import ConnectionIndicator from '@/components/ConnectionIndicator.vue'
import LanguageSwitcher from '@/components/LanguageSwitcher.vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useAuthStore } from '@/stores/auth'

const auth = useAuthStore()
const router = useRouter()
const { t } = useI18n()

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
      <div class="flex items-center gap-1 sm:gap-2">
        <LanguageSwitcher />
        <template v-if="auth.isAuthenticated">
          <ConnectionIndicator />
          <UButton v-if="auth.user?.systemAdmin" to="/admin" color="neutral" variant="ghost" icon="i-lucide-shield" class="hidden md:inline-flex">{{ t('header.admin') }}</UButton>
          <span class="hidden text-sm text-muted lg:inline">{{ auth.user?.displayName }}</span>
          <UButton color="neutral" variant="ghost" icon="i-lucide-log-out" :aria-label="t('header.signOut')" @click="logout" />
        </template>
      </div>
    </UContainer>
  </header>
</template>
