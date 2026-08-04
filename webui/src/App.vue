<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import { RouterView, useRoute } from 'vue-router'
import { Toaster } from 'vue-sonner'
import { motion } from 'motion-v'
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
const route = useRoute()
const collapsed = ref(false)
const mobileOpen = ref(false)
const showShell = computed(() => auth.isAuthenticated && route.name !== 'change-password')
const showActivity = computed(() => showShell.value && route.name !== 'logs' && (auth.user?.systemAdmin || auth.user?.canViewAudit))

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
  <div class="min-h-screen bg-[var(--app-bg)] text-[var(--app-text)]">
    <a href="#main-content" class="fixed left-3 top-3 z-[100] -translate-y-20 rounded bg-background px-3 py-2 text-xs text-foreground shadow focus:translate-y-0">{{ $t('navigation.browse') }}</a>
    <div v-if="showShell" class="flex h-dvh flex-col overflow-hidden lg:flex-row">
      <AppHeader shell :collapsed="collapsed" @menu="mobileOpen = true" @collapse="collapsed = !collapsed" />
      <div class="flex min-h-0 min-w-0 flex-1">
        <div v-if="!collapsed" class="hidden h-full shrink-0 lg:block"><FacilitySidebar /></div>
        <div class="flex min-w-0 flex-1 flex-col">
          <main id="main-content" class="min-h-0 min-w-0 flex-1 overflow-y-auto"><motion.div :key="route.fullPath" :initial="{ opacity: 0, y: 6 }" :animate="{ opacity: 1, y: 0 }" :transition="{ duration: 0.16 }"><RouterView /></motion.div></main>
          <ActivityLogBar v-if="showActivity" />
        </div>
      </div>
    </div>
    <template v-else>
      <AppHeader />
      <main id="main-content"><motion.div :key="route.fullPath" :initial="{ opacity: 0, y: 6 }" :animate="{ opacity: 1, y: 0 }" :transition="{ duration: 0.16 }"><RouterView /></motion.div></main>
    </template>
    <div v-if="mobileOpen && showShell" class="fixed inset-0 z-50 lg:hidden" @keydown.esc="mobileOpen = false"><button class="absolute inset-0 bg-black/50" :aria-label="$t('common.close')" @click="mobileOpen = false" /><motion.aside :initial="{ x: -24, opacity: 0 }" :animate="{ x: 0, opacity: 1 }" :transition="{ duration: 0.16 }" class="relative h-full w-[min(19rem,88vw)] shadow-2xl" role="dialog" aria-modal="true" :aria-label="$t('navigation.browse')"><FacilitySidebar @navigate="mobileOpen = false" /></motion.aside></div>
    <Toaster position="bottom-right" :close-button="true" />
  </div>
</template>
