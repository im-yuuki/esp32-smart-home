<script setup lang="ts">
import ConnectionIndicator from '@/components/ConnectionIndicator.vue'
import LanguageSwitcher from '@/components/LanguageSwitcher.vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useAuthStore } from '@/stores/auth'

const auth = useAuthStore()
const router = useRouter()
const { t } = useI18n()
defineProps<{ collapsed?: boolean }>()
const emit = defineEmits<{ menu: []; collapse: [] }>()

async function logout(): Promise<void> {
  try {
    await auth.logout()
  } finally {
    await router.replace({ name: 'login' })
  }
}
</script>

<template>
  <header class="sticky top-0 z-40 border-b border-default bg-default/95 backdrop-blur">
    <div class="flex h-14 items-center justify-between px-3 sm:px-5">
      <div class="flex items-center gap-2">
        <UButton v-if="auth.isAuthenticated" color="neutral" variant="ghost" icon="i-lucide-menu" class="lg:hidden" :aria-label="t('navigation.open')" @click="emit('menu')" />
        <UButton v-if="auth.isAuthenticated" color="neutral" variant="ghost" :icon="collapsed ? 'i-lucide-panel-left-open' : 'i-lucide-panel-left-close'" class="hidden lg:inline-flex" :aria-label="t('navigation.collapse')" @click="emit('collapse')" />
      <RouterLink to="/" class="flex items-center gap-2 font-semibold text-highlighted">
        <span class="grid size-7 place-items-center rounded bg-primary text-white"><UIcon name="i-lucide-building-2" class="size-4" /></span>
        <span class="hidden sm:inline">Facility Console</span>
      </RouterLink>
      </div>
      <div class="flex items-center gap-1 sm:gap-2">
        <LanguageSwitcher />
        <template v-if="auth.isAuthenticated">
          <ConnectionIndicator />
          <UButton v-if="auth.user?.systemAdmin" to="/admin" color="neutral" variant="ghost" icon="i-lucide-shield" class="hidden md:inline-flex">{{ t('header.admin') }}</UButton>
          <span class="hidden text-sm text-muted lg:inline">{{ auth.user?.displayName }}</span>
          <UButton color="neutral" variant="ghost" icon="i-lucide-log-out" :aria-label="t('header.signOut')" @click="logout" />
        </template>
      </div>
    </div>
  </header>
</template>
