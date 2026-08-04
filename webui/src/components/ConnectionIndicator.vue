<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRealtimeStore } from '@/stores/realtime'
import AppIcon from '@/components/AppIcon.vue'

const realtime = useRealtimeStore()
const { t } = useI18n()
defineProps<{ compact?: boolean }>()

const display = computed(() => {
  switch (realtime.status) {
    case 'connected':
      return { label: t('connection.live'), dot: 'status-dot--live', pulse: false }
    case 'connecting':
      return { label: t('connection.connecting'), dot: 'status-dot--waiting', pulse: true }
    case 'reconnecting':
      return { label: t('connection.reconnecting'), dot: 'status-dot--waiting', pulse: true }
    case 'disconnected':
    default:
       return { label: t('connection.offline'), dot: 'status-dot--offline', pulse: false }
  }
})

const tooltip = computed(() =>
  realtime.status === 'reconnecting'
    ? t('connection.reconnectAttempt', { count: realtime.attempts })
    : t('connection.status', { status: display.value.label }),
)
</script>

<template>
  <button type="button" class="flex items-center gap-2 rounded px-2 py-1 text-xs text-white/70" :aria-label="tooltip" :title="tooltip">
      <span
        class="size-2 rounded-full"
        :class="[display.dot, display.pulse ? 'animate-pulse' : '']"
      />
      <span class="hidden sm:inline" :class="{ 'lg:hidden': compact }">{{ display.label }}</span>
      <AppIcon name="i-lucide-activity" class="size-4 sm:hidden" />
  </button>
</template>
