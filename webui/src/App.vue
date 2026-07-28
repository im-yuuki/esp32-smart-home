<script setup lang="ts">
import { computed, onBeforeUnmount, watch } from 'vue'
import { RouterView } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { en, vi } from '@nuxt/ui/locale'
import AppHeader from '@/components/AppHeader.vue'
import { useWebSocket } from '@/composables/useWebSocket'
import { useAuthStore } from '@/stores/auth'
import { useNodesStore } from '@/stores/nodes'

// Realtime lifecycle is owned here — one STOMP client for the whole app.
const { connect, disconnect } = useWebSocket()
const auth = useAuthStore()
const nodes = useNodesStore()
const { locale } = useI18n()
const uiLocale = computed(() => locale.value === 'vi' ? vi : en)

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
      <AppHeader />
      <main>
        <RouterView />
      </main>
    </div>
  </UApp>
</template>
