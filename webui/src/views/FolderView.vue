<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import FacilityMap from '@/components/FacilityMap.vue'
import BulkActionPanel from '@/components/BulkActionPanel.vue'
import NodeCard from '@/components/NodeCard.vue'
import { getFolderMap } from '@/api/facilities'
import { useFacilitiesStore } from '@/stores/facilities'
import { useNodesStore } from '@/stores/nodes'
import { localizedError } from '@/i18n/errors'
import type { NodeInfo } from '@/types/api'
import type { CapabilityPlacement, FolderDto } from '@/types/facility'
import Button from '@/components/ui/Button.vue'
import Alert from '@/components/ui/Alert.vue'
import Skeleton from '@/components/ui/Skeleton.vue'
import AppIcon from '@/components/AppIcon.vue'

const route = useRoute()
const { t } = useI18n()
const facilities = useFacilitiesStore()
const nodeStore = useNodesStore()
const mode = ref<'map' | 'list'>('map')
const mapNodeIds = ref<string[]>([])
const placements = ref<CapabilityPlacement[]>([])
const mappedFolder = ref<FolderDto | null>(null)
const loading = ref(false)
const error = ref<string | null>(null)
const folderId = computed(() => String(route.params.folderId ?? ''))
const folder = computed(() => mappedFolder.value ?? facilities.folderById.get(folderId.value) ?? null)
const folderNodes = computed<NodeInfo[]>(() => mapNodeIds.value.map((id) => nodeStore.nodeById(id)).filter((node): node is NodeInfo => Boolean(node)))
const onlineCount = computed(() => folderNodes.value.filter((node) => node.online).length)
const pendingCount = computed(() => folderNodes.value.reduce((count, node) => count + node.relays.filter((relay) => relay.pending).length, 0))
const relayCount = computed(() => folderNodes.value.reduce((count, node) => count + node.relays.length, 0))
const breadcrumbs = computed(() => {
  const items: FolderDto[] = []
  let current = folder.value
  const seen = new Set<string>()
  while (current && !seen.has(current.id)) {
    items.unshift(current)
    seen.add(current.id)
    current = current.parentId ? facilities.folderById.get(current.parentId) ?? null : null
  }
  return items
})
let generation = 0

async function load() {
  const current = ++generation
  loading.value = true; error.value = null; mapNodeIds.value = []; placements.value = []; mappedFolder.value = null
  await facilities.load()
  try {
    const map = await getFolderMap(folderId.value)
    if (current !== generation) return
    nodeStore.mergeNodes(map.nodes)
    mappedFolder.value = map.folder
    mapNodeIds.value = map.nodes.map((node) => node.nodeId)
    placements.value = map.placements
  } catch (cause) { if (current === generation) error.value = localizedError(cause) } finally { if (current === generation) loading.value = false }
}
onMounted(load)
watch(folderId, load)
</script>

<template>
  <div class="min-h-full">
    <header class="border-b border-border bg-card px-4 py-4 sm:px-6">
      <nav class="mb-3 flex min-w-0 items-center gap-1 overflow-hidden text-[10px] uppercase tracking-[0.12em] text-muted-foreground" aria-label="Breadcrumb"><template v-for="(item, index) in breadcrumbs" :key="item.id"><AppIcon v-if="index" name="i-lucide-chevron-right" class="size-3 shrink-0" /><RouterLink :to="`/browse/${item.id}`" class="truncate hover:text-foreground">{{ item.name }}</RouterLink></template></nav>
      <div class="flex flex-wrap items-end justify-between gap-4">
        <div class="min-w-0"><div class="flex items-center gap-2"><span class="rounded border border-border px-1.5 py-0.5 font-mono text-[9px] text-muted-foreground">{{ folder?.templateType ?? t('browse.facility') }}</span><span class="font-mono text-[10px] text-muted-foreground">{{ folderId }}</span></div><h1 class="mt-2 truncate text-2xl font-semibold tracking-tight sm:text-3xl">{{ folder?.name ?? folderId }}</h1></div>
        <div class="flex flex-wrap items-center gap-2"><BulkActionPanel v-if="folder" :folder-id="folder.id" /><div class="flex rounded-md border border-border p-0.5"><button v-for="item in (['map', 'list'] as const)" :key="item" type="button" class="inline-flex h-8 items-center gap-2 rounded px-2 text-xs" :class="mode === item ? 'bg-foreground text-background' : 'text-muted-foreground hover:bg-muted'" @click="mode = item"><AppIcon :name="item === 'map' ? 'i-lucide-map' : 'i-lucide-list'" /><span>{{ t(`browse.${item}`) }}</span></button></div></div>
      </div>
      <dl class="mt-4 grid grid-cols-2 divide-x divide-border border-y border-border sm:grid-cols-4"><div class="px-3 py-2 first:pl-0"><dt class="text-[9px] uppercase tracking-wider text-muted-foreground">{{ t('browse.devices') }}</dt><dd class="mt-1 font-mono text-lg font-semibold">{{ folderNodes.length }}</dd></div><div class="px-3 py-2"><dt class="text-[9px] uppercase tracking-wider text-muted-foreground">{{ t('node.online') }}</dt><dd class="mt-1 font-mono text-lg font-semibold">{{ onlineCount }}</dd></div><div class="px-3 py-2"><dt class="text-[9px] uppercase tracking-wider text-muted-foreground">{{ t('node.relays') }}</dt><dd class="mt-1 font-mono text-lg font-semibold">{{ relayCount }}</dd></div><div class="px-3 py-2"><dt class="text-[9px] uppercase tracking-wider text-muted-foreground">{{ t('browse.pending') }}</dt><dd class="mt-1 font-mono text-lg font-semibold">{{ pendingCount }}</dd></div></dl>
    </header>
    <div class="p-4 sm:p-6">
      <div v-if="loading" class="space-y-3"><Skeleton class="h-[32rem] w-full" /><span class="sr-only">{{ t('browse.loading') }}</span></div>
      <div v-else-if="error" class="space-y-3"><Alert variant="destructive">{{ error }}</Alert><Button variant="ghost" @click="load"><AppIcon name="i-lucide-refresh-cw" />{{ t('common.retry') }}</Button></div>
      <template v-else-if="folder">
        <FacilityMap v-if="mode === 'map'" :folder="folder" :nodes="folderNodes" :placements="placements" />
        <section v-else-if="folderNodes.length" class="space-y-2"><NodeCard v-for="node in folderNodes" :key="node.nodeId" :node="node" /></section>
        <Alert v-else>{{ t('browse.noDevices') }}</Alert>
      </template>
    </div>
  </div>
</template>
