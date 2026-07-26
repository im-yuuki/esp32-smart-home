<script setup lang="ts">
import { onBeforeUnmount, onMounted } from 'vue'
import { RouterView } from 'vue-router'
import AppHeader from '@/components/AppHeader.vue'
import { useWebSocket } from '@/composables/useWebSocket'

// Realtime lifecycle is owned here — one STOMP client for the whole app.
const { connect, disconnect } = useWebSocket()

onMounted(connect)
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
