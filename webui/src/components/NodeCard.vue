<script setup lang="ts">
import { computed } from 'vue'
import type { NodeInfo, RelayChannel } from '@/types/api'
import NodeStatusBadge from '@/components/NodeStatusBadge.vue'
import RelaySwitch from '@/components/RelaySwitch.vue'
import SensorCard from '@/components/SensorCard.vue'
import { useNodesStore } from '@/stores/nodes'
import { useRealtimeStore } from '@/stores/realtime'

const props = defineProps<{ node: NodeInfo }>()

const nodesStore = useNodesStore()
const realtime = useRealtimeStore()

// A command sent while the WS is down could never be confirmed and would
// always hit the 5s timeout — prevent it; the header indicator explains why.
const controlsDisabled = computed(() => !props.node.online || !realtime.isConnected)

function onToggle(relay: RelayChannel): void {
  void nodesStore.sendRelayCommand(
    props.node.nodeId,
    relay.channel,
    relay.state === 'ON' ? 'OFF' : 'ON',
  )
}
</script>

<template>
  <UCard>
    <template #header>
      <div class="flex items-center justify-between gap-2">
        <div class="min-w-0">
          <p class="truncate font-medium text-highlighted">{{ node.nodeId }}</p>
          <p v-if="node.fwVersion" class="text-xs text-muted">fw {{ node.fwVersion }}</p>
        </div>
        <NodeStatusBadge :online="node.online" />
      </div>
    </template>

    <!-- Offline: dim + block the controls; disabled also covers WS-down. -->
    <div
      class="space-y-3 transition-opacity"
      :class="{ 'pointer-events-none opacity-50': !node.online }"
    >
      <div v-if="node.relays.length" class="divide-y divide-default">
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
      <SensorCard v-if="node.hasSensor" :sensor="node.sensor" />
    </div>

    <template #footer>
      <UButton
        :to="`/nodes/${node.nodeId}`"
        variant="link"
        color="primary"
        size="sm"
        trailing-icon="i-lucide-arrow-right"
        class="px-0"
      >
        Details
      </UButton>
    </template>
  </UCard>
</template>
