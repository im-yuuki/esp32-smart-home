<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import { RouterView, useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { en, vi } from '@nuxt/ui/locale'
import AppHeader from '@/components/AppHeader.vue'
import { useWebSocket } from '@/composables/useWebSocket'
import { useAuthStore } from '@/stores/auth'
import { useNodesStore } from '@/stores/nodes'
import FacilitySidebar from '@/components/FacilitySidebar.vue'
import ActivityLogBar from '@/components/ActivityLogBar.vue'

// Realtime lifecycle is owned here — one STOMP client for the whole app.
const { connect, disconnect } = useWebSocket()
const auth = useAuthStore()
const nodes = useNodesStore()
const { locale } = useI18n()
const uiLocale = computed(() => locale.value === 'vi' ? vi : en)
const route = useRoute()
const collapsed = ref(false)
const mobileOpen = ref(false)
const showShell = computed(() => auth.isAuthenticated && route.name !== 'change-password')
const showActivity = computed(() => showShell.value && (auth.user?.systemAdmin || auth.user?.canViewAudit))

watch(
  () => auth.isAuthenticated,
  (authenticated) => {
    if (authenticated) void connect()
    else {
      void disconnect()
      nodes.reset()
    }
  },
  { immediate: true },
)
onBeforeUnmount(() => {
  void disconnect()
})
</script>

<template>
  <!-- UApp provides the Toaster/Tooltip/overlay providers (required for useToast). -->
  <UApp :locale="uiLocale">
    <div class="min-h-screen bg-default text-default">
      <AppHeader :collapsed="collapsed" @menu="mobileOpen = true" @collapse="collapsed = !collapsed" />
      <div v-if="showShell" class="flex h-[calc(100dvh-3.5rem)] flex-col overflow-hidden">
        <div class="flex min-h-0 flex-1">
          <div class="hidden h-full shrink-0 lg:block"><FacilitySidebar :collapsed="collapsed" /></div>
          <main id="main-content" class="min-w-0 flex-1 overflow-y-auto"><RouterView /></main>
        </div>
        <ActivityLogBar v-if="showActivity" />
      </div>
      <main v-else id="main-content"><RouterView /></main>
      <div v-if="mobileOpen && showShell" class="fixed inset-0 z-50 lg:hidden"><button class="absolute inset-0 bg-slate-950/35" :aria-label="$t('common.close')" @click="mobileOpen = false" /><div class="relative h-full w-[min(19rem,88vw)] shadow-2xl"><FacilitySidebar @navigate="mobileOpen = false" /></div></div>
    </div>
  </UApp>
</template>
