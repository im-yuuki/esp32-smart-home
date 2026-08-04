<script setup lang="ts">
import { computed, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import type { NodeInfo, RelayChannel } from '@/types/api'
import type { CapabilityPlacement, FolderDto } from '@/types/facility'
import { useNodesStore } from '@/stores/nodes'
import { useRealtimeStore } from '@/stores/realtime'
import AppIcon from '@/components/AppIcon.vue'
import Button from '@/components/ui/Button.vue'

const props = defineProps<{ folder: FolderDto; nodes: NodeInfo[]; placements: CapabilityPlacement[] }>()
const store = useNodesStore()
const realtime = useRealtimeStore()
const { t } = useI18n()
const mapClass = computed(() => `facility-map--${props.folder.templateType.toLowerCase()}`)
const markers = computed(() => props.nodes.flatMap((node) => node.relays.map((relay) => ({ node, relay }))))
const selectedKey = ref<string | null>(null)
const keyOf = (node: NodeInfo, relay: RelayChannel) => `${node.nodeId}:${relay.id ?? relay.channel}`
const selected = computed(() => markers.value.find(({ node, relay }) => keyOf(node, relay) === selectedKey.value) ?? null)

function markerPlacement(relay: RelayChannel, index: number) {
  const placed = relay.id ? props.placements.find((item) => item.capabilityId === relay.id) : undefined
  const config = placed?.config ?? {}
  return {
    x: placed?.x ?? 16 + (index % 4) * 23,
    y: placed?.y ?? 25 + Math.floor(index / 4) * 24,
    rotation: typeof config.rotation === 'number' ? config.rotation : 0,
    scale: typeof config.scale === 'number' ? config.scale : 1,
    width: placed?.width ?? 10,
    height: placed?.height ?? 10,
    label: placed?.label || relay.displayName || relay.discoveryName || relay.name,
  }
}
function toggle(node: NodeInfo, relay: RelayChannel) {
  if (!node.online || !realtime.isConnected || relay.pending) return
  void store.sendRelayCommand(node.nodeId, relay.channel, relay.state === 'ON' ? 'OFF' : 'ON')
}
</script>

<template>
  <section class="facility-map" :class="mapClass" :aria-label="t('browse.mapLabel', { folder: folder.name })">
    <div class="map-grid" aria-hidden="true" />
    <div class="map-label"><span>{{ folder.templateType }}</span><strong>{{ folder.name }}</strong></div>
    <button v-for="({ node, relay }, index) in markers" :key="keyOf(node, relay)" type="button" class="device-marker" :class="{ 'is-on': relay.state === 'ON', 'is-offline': !node.online, 'is-pending': relay.pending }" :style="{ left: `${markerPlacement(relay, index).x}%`, top: `${markerPlacement(relay, index).y}%`, width: `${markerPlacement(relay, index).width}%`, minHeight: `${markerPlacement(relay, index).height}%`, transform: `translate(-50%, -50%) rotate(${markerPlacement(relay, index).rotation}deg) scale(${markerPlacement(relay, index).scale})` }" :aria-label="markerPlacement(relay, index).label" @click="selectedKey = keyOf(node, relay)">
       <span class="device-marker__icon"><AppIcon :name="relay.deviceType?.icon || 'i-lucide-power'" /></span>
      <span class="device-marker__label">{{ markerPlacement(relay, index).label }}</span>
      <span class="device-marker__state">{{ relay.pending ? t('browse.pending') : relay.state }} · {{ node.displayName || node.discoveryName || node.nodeId }}</span>
    </button>
    <aside v-if="selected" class="absolute bottom-3 right-3 z-10 w-[min(20rem,calc(100%-1.5rem))] border border-border bg-card p-4 text-card-foreground shadow-xl">
      <div class="flex items-start justify-between gap-3"><div class="min-w-0"><p class="section-kicker">{{ selected.relay.state }}</p><h2 class="truncate font-semibold">{{ selected.relay.displayName || selected.relay.name }}</h2><p class="truncate font-mono text-[10px] text-muted-foreground">{{ selected.node.displayName || selected.node.nodeId }}</p></div><Button variant="ghost" size="icon" :aria-label="t('common.close')" @click="selectedKey = null"><AppIcon name="i-lucide-x" /></Button></div>
      <div class="mt-4 flex items-center justify-between border-t border-border pt-3"><span class="text-xs text-muted-foreground">{{ selected.node.online ? t('node.online') : t('node.offline') }}</span><Button size="sm" :disabled="!selected.node.online || !realtime.isConnected || selected.relay.pending" :loading="selected.relay.pending" @click="toggle(selected.node, selected.relay)">{{ selected.relay.state === 'ON' ? 'OFF' : 'ON' }}</Button></div>
    </aside>
     <div v-if="!markers.length" class="absolute inset-0 grid place-items-center"><p class="rounded bg-[color:var(--app-bg)]/90 px-4 py-3 text-sm text-[var(--app-text-muted)]">{{ t('browse.noDevices') }}</p></div>
  </section>
</template>
