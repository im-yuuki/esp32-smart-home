<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRealtimeStore } from '@/stores/realtime'

const realtime = useRealtimeStore()
const { t } = useI18n()

const display = computed(() => {
  switch (realtime.status) {
    case 'connected':
      return { label: t('connection.live'), dot: 'bg-success', pulse: false }
    case 'connecting':
      return { label: t('connection.connecting'), dot: 'bg-warning', pulse: true }
    case 'reconnecting':
      return { label: t('connection.reconnecting'), dot: 'bg-warning', pulse: true }
    case 'disconnected':
    default:
      return { label: t('connection.offline'), dot: 'bg-error', pulse: false }
  }
})

const tooltip = computed(() =>
  realtime.status === 'reconnecting'
    ? t('connection.reconnectAttempt', { count: realtime.attempts })
    : t('connection.status', { status: display.value.label }),
)
</script>

<template>
  <UTooltip :text="tooltip">
    <div class="flex items-center gap-2 text-sm text-muted">
      <span
        class="size-2 rounded-full"
        :class="[display.dot, display.pulse ? 'animate-pulse' : '']"
      />
      <span class="hidden sm:inline">{{ display.label }}</span>
    </div>
  </UTooltip>
</template>
