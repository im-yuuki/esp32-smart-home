<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useRoute } from 'vue-router'
import NodeStatusBadge from '@/components/NodeStatusBadge.vue'
import RelaySwitch from '@/components/RelaySwitch.vue'
import SensorCard from '@/components/SensorCard.vue'
import SensorHistoryChart from '@/components/SensorHistoryChart.vue'
import { getSensorHistory } from '@/api/telemetry'
import { useNodesStore } from '@/stores/nodes'
import { useRealtimeStore } from '@/stores/realtime'
import type { RelayChannel, SensorSample } from '@/types/api'
import { absoluteTime } from '@/utils/time'
import { roomLabel } from '@/utils/rooms'

const route = useRoute()
const store = useNodesStore()
const realtime = useRealtimeStore()

const nodeId = computed(() => String(route.params.nodeId ?? ''))
const node = computed(() => store.nodeById(nodeId.value))

const controlsDisabled = computed(() => !node.value?.online || !realtime.isConnected)

// History is presentation state, not shared — a view-local buffer, not the store.
const samples = ref<SensorSample[]>([])
const historyLoading = ref(false)
const historyError = ref<string | null>(null)
let historyLoadedFor: string | null = null

async function loadHistory(): Promise<void> {
  const current = node.value
  if (!current?.hasSensor || historyLoadedFor === current.nodeId) return
  historyLoadedFor = current.nodeId
  historyLoading.value = true
  historyError.value = null
  try {
    // No from/to params -> the backend's now-24h .. now default window.
    samples.value = await getSensorHistory(current.nodeId)
  } catch (e) {
    historyError.value = e instanceof Error ? e.message : String(e)
    historyLoadedFor = null
  } finally {
    historyLoading.value = false
  }
}

onMounted(async () => {
  // Deep-link safety: direct navigation before any fetch completed.
  if (!store.loaded) await store.fetchNodes()
  await loadHistory()
})

// Re-arm when navigating between nodes, and load once the node materializes.
watch(nodeId, () => {
  samples.value = []
  historyLoadedFor = null
  void loadHistory()
})
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
      Dashboard
    </UButton>

    <!-- Still fetching the node list (deep link) -->
    <USkeleton v-if="!store.loaded && store.loading" class="h-64 w-full rounded-lg" />

    <UAlert
      v-else-if="!node"
      color="error"
      variant="subtle"
      icon="i-lucide-circle-alert"
      title="Node not found"
      :description="`No node with id '${nodeId}' is known to the server.`"
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
            <dt class="text-muted">Room</dt>
            <dd class="text-default">{{ roomLabel(node.room) }}</dd>
          </div>
          <div>
            <dt class="text-muted">Firmware</dt>
            <dd class="text-default">{{ node.fwVersion ?? '—' }}</dd>
          </div>
          <div>
            <dt class="text-muted">IP</dt>
            <dd class="text-default">{{ node.ip ?? '—' }}</dd>
          </div>
          <div>
            <dt class="text-muted">Last seen</dt>
            <dd class="text-default">{{ node.lastSeen ? absoluteTime(node.lastSeen) : '—' }}</dd>
          </div>
        </dl>
      </UCard>

      <UCard v-if="node.relays.length">
        <template #header>
          <h2 class="font-medium text-highlighted">Relays</h2>
        </template>
        <div
          class="divide-y divide-default transition-opacity"
          :class="{ 'pointer-events-none opacity-50': !node.online }"
        >
          <RelaySwitch
            v-for="relay in node.relays"
            :key="relay.channel"
            :label="relay.name"
            :state="relay.state"
            :pending="relay.pending"
            :disabled="controlsDisabled"
            @toggle="onToggle(relay)"
          />
        </div>
      </UCard>

      <UCard v-if="node.hasSensor">
        <template #header>
          <div class="flex items-center justify-between gap-2">
            <h2 class="font-medium text-highlighted">Temperature &amp; humidity — last 24h</h2>
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
            title="Failed to load history"
            :description="historyError"
          />
          <SensorHistoryChart v-else :samples="samples" />
        </div>
      </UCard>
    </template>
  </UContainer>
</template>
