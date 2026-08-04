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
import Card from '@/components/ui/Card.vue'
import Alert from '@/components/ui/Alert.vue'
import Skeleton from '@/components/ui/Skeleton.vue'
import AppIcon from '@/components/AppIcon.vue'

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
  <div class="mx-auto max-w-[1400px] space-y-5 p-4 sm:p-6">
    <RouterLink to="/" class="inline-flex items-center gap-2 text-xs font-medium underline-offset-4 hover:underline"><AppIcon name="i-lucide-arrow-left" />{{ t('common.dashboard') }}</RouterLink>

    <!-- Still fetching the node list (deep link) -->
    <Skeleton v-if="!store.loaded && store.loading" class="h-64 w-full" />

    <Alert v-else-if="!node" variant="destructive">{{ t('node.notFoundDescription', { nodeId }) }}</Alert>

    <template v-else>
      <Card class="overflow-hidden">
        <div>
          <div class="flex items-center justify-between gap-2 border-b border-border p-4 sm:p-5">
             <div class="min-w-0"><p class="section-kicker">{{ t('node.room') }} · {{ roomLabel(node.room, t) }}</p><h1 class="truncate text-2xl font-semibold">{{ node.displayName || node.discoveryName || node.nodeId }}</h1><p v-if="node.displayName || node.discoveryName" class="mt-1 truncate font-mono text-[10px] text-[var(--app-text-muted)]">{{ node.nodeId }}</p></div>
             <NodeStatusBadge :online="node.online" />
          </div>
        </div>
        <dl class="grid grid-cols-2 divide-x divide-border text-sm sm:grid-cols-4">
          <div class="p-4"><dt class="field-label">{{ t('node.room') }}</dt><dd>{{ roomLabel(node.room, t) }}</dd></div>
          <div>
             <dt class="field-label px-4 pt-4">{{ t('node.firmware') }}</dt>
             <dd class="px-4 pb-4 font-mono text-xs">{{ node.fwVersion ?? '—' }}</dd>
          </div>
          <div>
             <dt class="field-label px-4 pt-4">{{ t('node.ip') }}</dt>
             <dd class="px-4 pb-4 font-mono text-xs">{{ node.ip ?? '—' }}</dd>
          </div>
          <div>
             <dt class="field-label px-4 pt-4">{{ t('node.lastSeen') }}</dt>
             <dd class="px-4 pb-4 text-xs">{{ node.lastSeen ? absoluteTime(node.lastSeen, locale) : '—' }}</dd>
          </div>
        </dl>
      </Card>

      <div class="grid items-start gap-5 lg:grid-cols-[minmax(18rem,.7fr)_minmax(0,1.3fr)]">
      <Card v-if="node.relays.length" class="p-4 sm:p-5">
        <div>
          <h2 class="font-medium">{{ t('node.relays') }}</h2>
        </div>
        <div
             class="divide-y divide-[var(--app-border)] transition-opacity"
          :class="{ 'pointer-events-none opacity-50': !node.online }"
        >
          <RelaySwitch
            v-for="relay in node.relays"
            :key="relay.channel"
            :label="relay.displayName || relay.name || t('relay.fallbackName', { channel: relay.channel })"
            :state="relay.state"
            :pending="relay.pending"
            :disabled="controlsDisabled"
            @toggle="onToggle(relay)"
          />
        </div>
      </Card>

      <Card v-if="node.hasSensor && node.permissions.includes(TELEMETRY_VIEW)" class="p-4 sm:p-5">
        <div>
          <div class="flex items-center justify-between gap-2">
             <h2 class="font-medium">{{ t('node.sensorHistory') }}</h2>
             <span v-if="node.sensorMeta?.model" class="text-xs text-[var(--app-text-muted)]">
              {{ node.sensorMeta.model }}
            </span>
          </div>
        </div>
        <div class="space-y-4">
          <SensorCard :sensor="node.sensor" />
           <Skeleton v-if="historyLoading" class="h-72 w-full" />
           <Alert v-else-if="historyError" variant="destructive">{{ historyError }}</Alert>
          <SensorHistoryChart v-else :samples="samples" />
        </div>
      </Card>
      </div>
    </template>
  </div>
</template>
