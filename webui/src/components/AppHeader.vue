<script setup lang="ts">
import Button from '@/components/ui/Button.vue'
import ConnectionIndicator from '@/components/ConnectionIndicator.vue'
import LanguageSwitcher from '@/components/LanguageSwitcher.vue'
import ThemeSwitcher from '@/components/ThemeSwitcher.vue'
import AppIcon from '@/components/AppIcon.vue'
import UserMenu from '@/components/UserMenu.vue'
import { useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useAuthStore } from '@/stores/auth'

const auth = useAuthStore()
const route = useRoute()
const { t } = useI18n()
defineProps<{ collapsed?: boolean; shell?: boolean }>()
const emit = defineEmits<{ menu: []; collapse: [] }>()

</script>

<template>
  <header class="z-40 flex h-14 shrink-0 border-black bg-black text-white" :class="shell ? 'border-b lg:h-full lg:w-16 lg:flex-col lg:border-b-0 lg:border-r' : 'sticky top-0 border-b'">
    <div class="flex h-full w-full items-center justify-between gap-2 px-3" :class="shell ? 'lg:flex-col lg:px-2 lg:py-3' : 'sm:px-5'">
      <div class="flex items-center gap-2" :class="shell ? 'lg:flex-col' : ''">
        <Button v-if="shell" variant="ghost" size="icon" class="text-white hover:bg-white/15 lg:hidden" :aria-label="t('navigation.open')" @click="emit('menu')"><AppIcon name="i-lucide-menu" /></Button>
        <RouterLink to="/" class="flex items-center gap-2 font-semibold text-white" :title="t('common.dashboard')" :aria-label="t('common.dashboard')">
          <span class="grid size-8 place-items-center rounded-md bg-white text-black"><AppIcon name="i-lucide-building-2" class="size-4" /></span>
          <span class="text-sm sm:inline" :class="shell ? 'lg:hidden' : ''">Facility Console</span>
        </RouterLink>
        <Button v-if="shell" variant="ghost" size="icon" class="hidden text-white hover:bg-white/15 lg:inline-flex" :aria-label="t('navigation.collapse')" @click="emit('collapse')"><AppIcon :name="collapsed ? 'i-lucide-panel-left-open' : 'i-lucide-panel-left-close'" /></Button>
      </div>

      <nav v-if="shell && auth.isAuthenticated" class="hidden flex-1 flex-col items-center justify-center gap-1 lg:flex" :aria-label="t('navigation.browse')">
        <RouterLink to="/" class="grid size-10 place-items-center rounded-md text-white/55 hover:bg-white/10 hover:text-white" :class="route.name === 'browse' || route.name === 'dashboard' ? 'bg-white !text-black hover:bg-white hover:!text-black' : ''" :title="t('common.dashboard')" :aria-label="t('common.dashboard')"><AppIcon name="i-lucide-layout-dashboard" /></RouterLink>
        <RouterLink v-if="auth.user?.systemAdmin || auth.user?.canViewAudit" to="/logs" class="grid size-10 place-items-center rounded-md text-white/55 hover:bg-white/10 hover:text-white" :class="route.name === 'logs' ? 'bg-white !text-black hover:bg-white hover:!text-black' : ''" :title="t('navigation.logs')" :aria-label="t('navigation.logs')"><AppIcon name="i-lucide-scroll-text" /></RouterLink>
        <RouterLink v-if="auth.user?.systemAdmin" to="/admin" class="grid size-10 place-items-center rounded-md text-white/55 hover:bg-white/10 hover:text-white" :class="route.name === 'admin' ? 'bg-white !text-black hover:bg-white hover:!text-black' : ''" :title="t('header.admin')" :aria-label="t('header.admin')"><AppIcon name="i-lucide-shield" /></RouterLink>
      </nav>

      <div class="flex items-center gap-1" :class="shell ? 'lg:flex-col' : ''">
        <ConnectionIndicator v-if="shell && auth.isAuthenticated" compact />
        <LanguageSwitcher />
        <ThemeSwitcher />
        <UserMenu v-if="auth.isAuthenticated" />
      </div>
    </div>
  </header>
</template>
