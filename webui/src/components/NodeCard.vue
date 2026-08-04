<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { NODE_CONTROL, TELEMETRY_VIEW, type NodeInfo, type RelayChannel } from '@/types/api'
import NodeStatusBadge from '@/components/NodeStatusBadge.vue'
import RelaySwitch from '@/components/RelaySwitch.vue'
import { useNodesStore } from '@/stores/nodes'
import { useRealtimeStore } from '@/stores/realtime'
import AppIcon from '@/components/AppIcon.vue'
import Card from '@/components/ui/Card.vue'
import Badge from '@/components/ui/Badge.vue'

const props = defineProps<{ node: NodeInfo }>()

const nodesStore = useNodesStore()
const realtime = useRealtimeStore()
const { n, t } = useI18n()

// A command sent while the WS is down could never be confirmed and would
// always hit the 5s timeout — prevent it; the header indicator explains why.
const controlsDisabled = computed(() =>
  !props.node.permissions.includes(NODE_CONTROL) || !props.node.online || !realtime.isConnected,
)

function onToggle(relay: RelayChannel): void {
  void nodesStore.sendRelayCommand(
    props.node.nodeId,
    relay.channel,
    relay.state === 'ON' ? 'OFF' : 'ON',
  )
}
</script>

<template>
  <Card class="device-card overflow-hidden">
    <div class="grid lg:grid-cols-[minmax(14rem,1.1fr)_minmax(15rem,1fr)_10rem_3rem]">
      <div class="min-w-0 p-4">
        <div class="flex items-start justify-between gap-2"><div class="min-w-0"><p class="truncate font-medium">{{ node.displayName || node.discoveryName || node.nodeId }}</p><p class="mt-1 truncate font-mono text-[10px] text-muted-foreground">{{ node.nodeId }}<template v-if="node.fwVersion"> · {{ t('node.firmwareShort', { version: node.fwVersion }) }}</template></p></div><NodeStatusBadge :online="node.online" /></div>
        <div class="mt-3 flex flex-wrap gap-1.5"><template v-for="relay in node.relays" :key="`meta-${relay.channel}`"><Badge v-if="relay.deviceType" variant="outline">{{ relay.deviceType.name }}</Badge><Badge v-for="tag in relay.tags" :key="tag.id" variant="secondary">{{ tag.name }}</Badge></template></div>
      </div>
      <div class="border-t border-border px-4 py-2 lg:border-l lg:border-t-0" :class="{ 'opacity-45': !node.online }"><RelaySwitch v-for="relay in node.relays" :key="relay.channel" :label="relay.displayName || relay.name || t('relay.fallbackName', { channel: relay.channel })" :state="relay.state" :pending="relay.pending" :disabled="controlsDisabled" @toggle="onToggle(relay)" /><p v-if="!node.relays.length" class="py-3 text-xs text-muted-foreground">{{ t('common.none') }}</p></div>
      <div class="border-t border-border p-4 lg:border-l lg:border-t-0"><p class="field-label">{{ t('node.sensorHistory') }}</p><div v-if="node.hasSensor && node.permissions.includes(TELEMETRY_VIEW)" class="flex items-baseline gap-3"><strong class="font-mono text-lg">{{ node.sensor ? n(node.sensor.temperature, 'oneDecimal') : '—' }}°</strong><span class="font-mono text-xs text-muted-foreground">{{ node.sensor ? n(node.sensor.humidity, 'integer') : '—' }}%</span></div><span v-else class="text-xs text-muted-foreground">—</span></div>
      <RouterLink :to="`/nodes/${node.nodeId}`" class="grid min-h-12 place-items-center border-t border-border hover:bg-muted lg:border-l lg:border-t-0" :aria-label="t('node.details')"><AppIcon name="i-lucide-arrow-right" /></RouterLink>
    </div>
  </Card>
</template>
