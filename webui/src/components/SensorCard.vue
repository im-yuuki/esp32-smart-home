<script setup lang="ts">
import { onMounted, onUnmounted, ref } from 'vue'
import type { SensorReading } from '@/types/api'
import { relativeTime } from '@/utils/time'

defineProps<{ sensor: SensorReading | null }>()

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
  <div class="rounded-lg bg-elevated/50 p-3">
    <div class="flex items-end justify-between gap-4">
      <div class="flex items-baseline gap-1">
        <UIcon name="i-lucide-thermometer" class="size-4 self-center text-muted" />
        <span class="text-2xl font-semibold tabular-nums text-highlighted">
          {{ sensor ? sensor.temperature.toFixed(1) : '—' }}
        </span>
        <span class="text-sm text-muted">°C</span>
      </div>
      <div class="flex items-baseline gap-1">
        <UIcon name="i-lucide-droplets" class="size-4 self-center text-muted" />
        <span class="text-2xl font-semibold tabular-nums text-highlighted">
          {{ sensor ? Math.round(sensor.humidity) : '—' }}
        </span>
        <span class="text-sm text-muted">%</span>
      </div>
    </div>
    <p class="mt-1 text-xs text-muted">
      {{ sensor ? `updated ${relativeTime(sensor.ts, now)}` : 'no reading yet' }}
    </p>
  </div>
</template>
