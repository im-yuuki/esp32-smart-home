<script setup lang="ts">
import { onBeforeUnmount, watch } from 'vue'
import { RouterView } from 'vue-router'
import AppHeader from '@/components/AppHeader.vue'
import { useWebSocket } from '@/composables/useWebSocket'
import { useAuthStore } from '@/stores/auth'
import { useNodesStore } from '@/stores/nodes'

// Realtime lifecycle is owned here — one STOMP client for the whole app.
const { connect, disconnect } = useWebSocket()
const auth = useAuthStore()
const nodes = useNodesStore()

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
  <UApp>
    <div class="min-h-screen bg-default text-default">
      <AppHeader />
      <main>
        <RouterView />
      </main>
    </div>
  </UApp>
</template>
