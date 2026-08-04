<script setup lang="ts">
import { onMounted, onUnmounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import type { SensorReading } from '@/types/api'
import { relativeTime } from '@/utils/time'
import AppIcon from '@/components/AppIcon.vue'

defineProps<{ sensor: SensorReading | null }>()
const { locale, n, t } = useI18n()

// 1s ticker so "updated Xs ago" stays live.
const now = ref(Date.now())
let ticker = 0

onMounted(() => {
  ticker = window.setInterval(() => {
    now.value = Date.now()
  }, 1_000)
})

onUnmounted(() => {
  window.clearInterval(ticker)
})
</script>

<template>
  <div class="border border-[var(--app-border)] bg-[var(--app-surface-muted)] p-3">
    <div class="flex items-end justify-between gap-4">
      <div class="flex items-baseline gap-1">
        <AppIcon name="i-lucide-thermometer" class="size-4 self-center text-[var(--app-text-muted)]" />
        <span class="text-2xl font-semibold tabular-nums">
          {{ sensor ? n(sensor.temperature, 'oneDecimal') : '—' }}
        </span>
        <span class="text-sm text-[var(--app-text-muted)]">°C</span>
      </div>
      <div class="flex items-baseline gap-1">
        <AppIcon name="i-lucide-droplets" class="size-4 self-center text-[var(--app-text-muted)]" />
        <span class="text-2xl font-semibold tabular-nums">
          {{ sensor ? n(sensor.humidity, 'integer') : '—' }}
        </span>
        <span class="text-sm text-[var(--app-text-muted)]">%</span>
      </div>
    </div>
    <p class="mt-1 text-xs text-[var(--app-text-muted)]">
      {{ sensor ? t('sensor.updated', { time: relativeTime(sensor.ts, now, locale) }) : t('sensor.noReading') }}
    </p>
  </div>
</template>
