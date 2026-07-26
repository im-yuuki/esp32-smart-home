<script setup lang="ts">
import { computed } from 'vue'
import { useRealtimeStore } from '@/stores/realtime'

const realtime = useRealtimeStore()

const display = computed(() => {
  switch (realtime.status) {
    case 'connected':
      return { label: 'Live', dot: 'bg-success', pulse: false }
    case 'connecting':
      return { label: 'Connecting…', dot: 'bg-warning', pulse: true }
    case 'reconnecting':
      return { label: 'Reconnecting…', dot: 'bg-warning', pulse: true }
    case 'disconnected':
    default:
      return { label: 'Offline', dot: 'bg-error', pulse: false }
  }
})

const tooltip = computed(() =>
  realtime.status === 'reconnecting'
    ? `Reconnect attempt ${realtime.attempts}`
    : `Realtime connection: ${display.value.label}`,
)
</script>

<template>
  <UTooltip :text="tooltip">
    <div class="flex items-center gap-2 text-sm text-muted">
      <span
        class="size-2 rounded-full"
        :class="[display.dot, display.pulse ? 'animate-pulse' : '']"
      />
      <span>{{ display.label }}</span>
    </div>
  </UTooltip>
</template>
