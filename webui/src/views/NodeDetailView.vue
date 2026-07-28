<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import NodeStatusBadge from '@/components/NodeStatusBadge.vue'
import RelaySwitch from '@/components/RelaySwitch.vue'
import SensorCard from '@/components/SensorCard.vue'
import SensorHistoryChart from '@/components/SensorHistoryChart.vue'
import { getSensorHistory } from '@/api/telemetry'
import { useNodesStore } from '@/stores/nodes'
import { useRealtimeStore } from '@/stores/realtime'
import { NODE_CONTROL, TELEMETRY_VIEW, type RelayChannel, type SensorSample } from '@/types/api'
import { localizedError } from '@/i18n/errors'
import { absoluteTime } from '@/utils/time'
import { roomLabel } from '@/utils/rooms'

const route = useRoute()
const store = useNodesStore()
const realtime = useRealtimeStore()
const { locale, t } = useI18n()

const nodeId = computed(() => String(route.params.nodeId ?? ''))
const node = computed(() => store.nodeById(nodeId.value))

const controlsDisabled = computed(() =>
  !node.value?.permissions.includes(NODE_CONTROL) || !node.value.online || !realtime.isConnected,
)

// History is presentation state, not shared — a view-local buffer, not the store.
const samples = ref<SensorSample[]>([])
const historyLoading = ref(false)
const historyError = ref<string | null>(null)
let historyLoadedFor: string | null = null
let historyGeneration = 0

async function loadHistory(): Promise<void> {
  const current = node.value
  if (!current?.hasSensor || !current.permissions.includes(TELEMETRY_VIEW)
      || historyLoadedFor === current.nodeId) return
  historyLoadedFor = current.nodeId
  const generation = historyGeneration
  const requestedNodeId = current.nodeId
  historyLoading.value = true
  historyError.value = null
  try {
    // No from/to params -> the backend's now-24h .. now default window.
    const result = await getSensorHistory(current.nodeId)
    if (generation !== historyGeneration || nodeId.value !== requestedNodeId) return
    samples.value = result
  } catch (e) {
    if (generation !== historyGeneration || nodeId.value !== requestedNodeId) return
    historyError.value = localizedError(e)
    historyLoadedFor = null
  } finally {
    if (generation === historyGeneration && nodeId.value === requestedNodeId) historyLoading.value = false
  }
}

onMounted(async () => {
  // Deep-link safety: direct navigation before any fetch completed.
  if (!store.loaded) await store.fetchNodes()
  await loadHistory()
})

// Re-arm when navigating between nodes, and load once the node materializes.
watch(nodeId, () => {
  historyGeneration++
  samples.value = []
  historyLoadedFor = null
  void loadHistory()
})
onBeforeUnmount(() => { historyGeneration++ })
watch(node, () => {
  void loadHistory()
})

// Live append: SENSOR_STATE events land in the store; mirror them into the buffer.
watch(
  () => node.value?.sensor,
  (reading) => {
    if (!reading) return
    const last = samples.value[samples.value.length - 1]
    if (last && last.ts >= reading.ts) return // dedupe (history tail / repeats)
    samples.value.push({
      ts: reading.ts,
      temperature: reading.temperature,
      humidity: reading.humidity,
    })
  },
)

function onToggle(relay: RelayChannel): void {
  const current = node.value
  if (!current) return
  void store.sendRelayCommand(
    current.nodeId,
    relay.channel,
    relay.state === 'ON' ? 'OFF' : 'ON',
  )
}
</script>

<template>
  <UContainer class="space-y-6 py-6">
    <UButton
      to="/"
      variant="link"
      color="neutral"
      icon="i-lucide-arrow-left"
      size="sm"
      class="px-0"
    >
      {{ t('common.dashboard') }}
    </UButton>

    <!-- Still fetching the node list (deep link) -->
    <USkeleton v-if="!store.loaded && store.loading" class="h-64 w-full rounded-lg" />

    <UAlert
      v-else-if="!node"
      color="error"
      variant="subtle"
      icon="i-lucide-circle-alert"
      :title="t('node.notFound')"
      :description="t('node.notFoundDescription', { nodeId })"
    />

    <template v-else>
      <UCard>
        <template #header>
          <div class="flex items-center justify-between gap-2">
            <h1 class="truncate text-xl font-semibold text-highlighted">{{ node.nodeId }}</h1>
            <NodeStatusBadge :online="node.online" />
          </div>
        </template>
        <dl class="grid grid-cols-2 gap-x-4 gap-y-2 text-sm sm:grid-cols-4">
          <div>
            <dt class="text-muted">{{ t('node.room') }}</dt>
            <dd class="text-default">{{ roomLabel(node.room, t) }}</dd>
          </div>
          <div>
            <dt class="text-muted">{{ t('node.firmware') }}</dt>
            <dd class="text-default">{{ node.fwVersion ?? '—' }}</dd>
          </div>
          <div>
            <dt class="text-muted">{{ t('node.ip') }}</dt>
            <dd class="text-default">{{ node.ip ?? '—' }}</dd>
          </div>
          <div>
            <dt class="text-muted">{{ t('node.lastSeen') }}</dt>
            <dd class="text-default">{{ node.lastSeen ? absoluteTime(node.lastSeen, locale) : '—' }}</dd>
          </div>
        </dl>
      </UCard>

      <UCard v-if="node.relays.length">
        <template #header>
          <h2 class="font-medium text-highlighted">{{ t('node.relays') }}</h2>
        </template>
        <div
          class="divide-y divide-default transition-opacity"
          :class="{ 'pointer-events-none opacity-50': !node.online }"
        >
          <RelaySwitch
            v-for="relay in node.relays"
            :key="relay.channel"
            :label="relay.name || t('relay.fallbackName', { channel: relay.channel })"
            :state="relay.state"
            :pending="relay.pending"
            :disabled="controlsDisabled"
            @toggle="onToggle(relay)"
          />
        </div>
      </UCard>

      <UCard v-if="node.hasSensor && node.permissions.includes(TELEMETRY_VIEW)">
        <template #header>
          <div class="flex items-center justify-between gap-2">
            <h2 class="font-medium text-highlighted">{{ t('node.sensorHistory') }}</h2>
            <span v-if="node.sensorMeta?.model" class="text-xs text-muted">
              {{ node.sensorMeta.model }}
            </span>
          </div>
        </template>
        <div class="space-y-4">
          <SensorCard :sensor="node.sensor" />
          <USkeleton v-if="historyLoading" class="h-72 w-full rounded-lg" />
          <UAlert
            v-else-if="historyError"
            color="error"
            variant="subtle"
            icon="i-lucide-circle-alert"
            :title="t('node.historyFailed')"
            :description="historyError"
          />
          <SensorHistoryChart v-else :samples="samples" />
        </div>
      </UCard>
    </template>
  </UContainer>
</template>
