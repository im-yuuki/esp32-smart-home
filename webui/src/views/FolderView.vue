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
  <div class="mx-auto max-w-[1500px] space-y-5 p-4 sm:p-6">
    <header class="flex flex-wrap items-end justify-between gap-4 border-b border-default pb-5">
      <div class="min-w-0"><p class="mb-1 text-xs font-semibold uppercase tracking-[0.15em] text-primary">{{ folder?.templateType ?? t('browse.facility') }}</p><h1 class="truncate text-2xl font-semibold tracking-tight text-highlighted sm:text-3xl">{{ folder?.name ?? folderId }}</h1><p class="mt-1 text-sm text-muted">{{ t('browse.deviceCount', { count: folderNodes.length }) }}</p></div>
      <div class="flex flex-wrap items-center gap-2"><BulkActionPanel v-if="folder" :folder-id="folder.id" /><div class="flex border border-default bg-muted p-1"><button v-for="item in (['map', 'list'] as const)" :key="item" type="button" class="segmented" :class="{ active: mode === item }" @click="mode = item"><UIcon :name="item === 'map' ? 'i-lucide-map' : 'i-lucide-list'" />{{ t(`browse.${item}`) }}</button></div></div>
    </header>
    <div v-if="loading" class="space-y-3"><USkeleton class="h-[32rem] w-full" /><span class="sr-only">{{ t('browse.loading') }}</span></div>
    <div v-else-if="error" class="space-y-3"><UAlert color="error" variant="subtle" :title="t('browse.loadFailed')" :description="error" /><UButton icon="i-lucide-refresh-cw" @click="load">{{ t('common.retry') }}</UButton></div>
    <template v-else-if="folder">
      <FacilityMap v-if="mode === 'map'" :folder="folder" :nodes="folderNodes" :placements="placements" />
      <section v-else-if="folderNodes.length" class="grid gap-4 sm:grid-cols-2 xl:grid-cols-3"><NodeCard v-for="node in folderNodes" :key="node.nodeId" :node="node" /></section>
      <UAlert v-else color="neutral" variant="subtle" icon="i-lucide-radio-tower" :title="t('browse.emptyTitle')" :description="t('browse.noDevices')" />
    </template>
  </div>
</template>
