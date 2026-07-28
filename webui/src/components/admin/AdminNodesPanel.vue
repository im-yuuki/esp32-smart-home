<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import * as api from '@/api/admin'
import { getFolderMap } from '@/api/facilities'
import { useFacilitiesStore } from '@/stores/facilities'
import { useNodesStore } from '@/stores/nodes'
import { localizedError } from '@/i18n/errors'
import type { AdminNode } from '@/types/admin'
import type { CapabilityPlacement, CapabilityPlacementMutation } from '@/types/facility'

const { t } = useI18n()
const facilities = useFacilitiesStore()
const nodes = useNodesStore()
const pending = ref<AdminNode[]>([])
const selectedId = ref<string | null>(null)
const selectedRelayId = ref<string | null>(null)
const approvalFolders = ref<Record<string, string>>({})
const placements = ref<CapabilityPlacement[]>([])
const error = ref<string | null>(null)
const busy = ref(false)
const displayName = ref('')
const newDeviceTypeName = ref('')
const newDeviceTypeDescription = ref('')
const newTagName = ref('')
const newTagColor = ref('#3f7d62')
const selected = computed(() => selectedId.value ? nodes.nodeById(selectedId.value) : undefined)
const selectedRelay = computed(() => selected.value?.relays.find((relay) => relay.id === selectedRelayId.value))
const placement = ref<CapabilityPlacementMutation>({ x: 50, y: 50, width: 10, height: 10, sortOrder: 0, config: { rotation: 0, scale: 1 } })
const rotation = computed({ get: () => typeof placement.value.config.rotation === 'number' ? placement.value.config.rotation : 0, set: (value: number) => { placement.value.config.rotation = value } })
const scale = computed({ get: () => typeof placement.value.config.scale === 'number' ? placement.value.config.scale : 1, set: (value: number) => { placement.value.config.scale = value } })

function syncPlacement() {
  const current = selectedRelayId.value ? placements.value.find((item) => item.capabilityId === selectedRelayId.value) : null
  placement.value = current ? { x: current.x, y: current.y, width: current.width, height: current.height, sortOrder: current.sortOrder, config: JSON.parse(JSON.stringify(current.config)) as Record<string, unknown> } : { x: 50, y: 50, width: 10, height: 10, sortOrder: 0, config: { rotation: 0, scale: 1 } }
}
async function loadPlacements() {
  const node = selected.value
  placements.value = []
  if (!node?.folderId) { syncPlacement(); return }
  try {
    const map = await getFolderMap(node.folderId)
    nodes.mergeNodes(map.nodes)
    placements.value = map.placements
  } catch (cause) { error.value = localizedError(cause) }
  syncPlacement()
}
async function load() {
  busy.value = true; error.value = null
  try {
    pending.value = await api.listAdminNodes('PENDING')
    await nodes.fetchNodes()
    if (!selectedId.value || !nodes.nodeById(selectedId.value)) selectedId.value = [...nodes.nodes.keys()][0] ?? null
  } catch (cause) { error.value = localizedError(cause) } finally { busy.value = false }
}
async function run(action: () => Promise<void>) { error.value = null; busy.value = true; try { await action(); await load() } catch (cause) { error.value = localizedError(cause); busy.value = false } }
async function saveDisplayName() { const node = selected.value; if (!node) return; await run(() => api.patchNodeDisplayName(node.nodeId, displayName.value.trim() || null)) }
async function savePlacement() { const node = selected.value; const relay = selectedRelay.value; if (!node?.folderId || !relay?.id) return; busy.value = true; error.value = null; try { await api.updatePlacement(node.folderId, relay.id, placement.value); await loadPlacements() } catch (cause) { error.value = localizedError(cause) } finally { busy.value = false } }
async function createDeviceType() { if (!newDeviceTypeName.value.trim()) return; await run(() => api.createDeviceType(newDeviceTypeName.value, newDeviceTypeDescription.value)); newDeviceTypeName.value = ''; newDeviceTypeDescription.value = ''; await facilities.load(true) }
async function createTag() { if (!newTagName.value.trim()) return; await run(() => api.createTag(newTagName.value, newTagColor.value)); newTagName.value = ''; await facilities.load(true) }

watch([() => selected.value?.nodeId, () => selected.value?.folderId], () => { displayName.value = selected.value?.displayName ?? ''; selectedRelayId.value = selected.value?.relays.find((relay) => relay.id)?.id ?? null; void loadPlacements() }, { immediate: true })
watch(selectedRelayId, syncPlacement)
onMounted(load)
</script>

<template>
  <div class="space-y-6">
    <UAlert v-if="error" color="error" variant="subtle" :description="error" />
    <section class="grid gap-5 border-y border-default bg-muted/40 p-4 md:grid-cols-2">
      <form class="space-y-3" @submit.prevent="createDeviceType"><h2 class="font-semibold">{{ t('admin.nodes.deviceTypeCatalog') }}</h2><div class="grid gap-2 sm:grid-cols-2"><UInput v-model="newDeviceTypeName" :placeholder="t('admin.nodes.deviceTypeName')" required /><UInput v-model="newDeviceTypeDescription" :placeholder="t('admin.nodes.description')" /></div><UButton type="submit" size="sm" color="neutral" variant="outline" :loading="busy">{{ t('admin.nodes.addDeviceType') }}</UButton></form>
      <form class="space-y-3" @submit.prevent="createTag"><h2 class="font-semibold">{{ t('admin.nodes.tagCatalog') }}</h2><div class="grid grid-cols-[minmax(0,1fr)_3rem] gap-2"><UInput v-model="newTagName" :placeholder="t('admin.nodes.tagName')" required /><input v-model="newTagColor" type="color" class="h-8 w-full border border-default bg-default" :aria-label="t('admin.nodes.tagColor')"></div><UButton type="submit" size="sm" color="neutral" variant="outline" :loading="busy">{{ t('admin.nodes.addTag') }}</UButton></form>
    </section>
    <section><h2 class="mb-3 text-lg font-semibold">{{ t('admin.nodes.pending') }}</h2><div v-if="!pending.length" class="empty-panel">{{ t('admin.nodes.noPending') }}</div><div v-for="node in pending" :key="node.nodeId" class="flex flex-wrap items-center gap-3 border-b border-default py-3"><div class="min-w-48 flex-1"><strong class="block">{{ node.displayName || node.discoveryName || node.nodeId }}</strong><span class="text-xs text-muted">{{ node.nodeId }} · {{ node.ip }}</span></div><select v-model="approvalFolders[node.nodeId]" class="form-control"><option value="" disabled>{{ t('admin.nodes.chooseFolder') }}</option><option v-for="folder in facilities.folders" :key="folder.id" :value="folder.id">{{ folder.name }}</option></select><UButton size="sm" :disabled="!approvalFolders[node.nodeId]" @click="run(() => api.approveNodeToFolder(node.nodeId, approvalFolders[node.nodeId]!))">{{ t('admin.nodes.approve') }}</UButton></div></section>
    <section class="admin-grid">
      <div class="admin-list"><h2 class="px-3 py-3 font-semibold">{{ t('admin.nodes.approved') }}</h2><button v-for="node in nodes.nodes.values()" :key="node.nodeId" type="button" class="admin-list-row" :class="{ active: selectedId === node.nodeId }" @click="selectedId = node.nodeId"><span class="size-2 rounded-full" :class="node.online ? 'bg-primary' : 'bg-neutral-400'" /><span class="truncate">{{ node.displayName || node.discoveryName || node.nodeId }}</span></button></div>
      <div v-if="selected" class="space-y-7 p-4 sm:p-6">
        <form class="space-y-4" @submit.prevent="saveDisplayName"><div><p class="section-kicker">{{ selected.nodeId }}</p><h2 class="text-xl font-semibold">{{ displayName || selected.discoveryName || selected.nodeId }}</h2></div><div class="grid gap-4 sm:grid-cols-2"><UFormField :label="t('admin.nodes.displayName')"><UInput v-model="displayName" class="w-full" /></UFormField><UFormField :label="t('admin.nodes.folder')"><select :value="selected.folderId" class="form-control w-full" @change="run(() => api.setNodeFolder(selected!.nodeId, ($event.target as HTMLSelectElement).value))"><option v-for="folder in facilities.folders" :key="folder.id" :value="folder.id">{{ folder.name }}</option></select></UFormField><div><span class="field-label">{{ t('node.room') }}</span><p class="py-2 text-sm">{{ selected.room }}</p></div></div><UButton type="submit" :loading="busy">{{ t('common.save') }}</UButton></form>
        <section><h3 class="mb-3 font-semibold">{{ t('admin.nodes.capabilities') }}</h3><div v-for="relay in selected.relays" :key="relay.id || relay.channel" class="grid gap-3 border-t border-default py-4 sm:grid-cols-3"><UInput :model-value="relay.displayName ?? relay.discoveryName" @update:model-value="relay.displayName = String($event)" /><select :value="relay.deviceType?.id" class="form-control" @change="relay.deviceType = facilities.deviceTypes.find(item => item.id === ($event.target as HTMLSelectElement).value) ?? null"><option value="">{{ t('common.none') }}</option><option v-for="type in facilities.deviceTypes" :key="type.id" :value="type.id">{{ type.name }}</option></select><UButton type="button" color="neutral" variant="outline" :disabled="!relay.id" @click="relay.id && run(() => api.patchCapabilityMetadata(relay.id!, { displayName: relay.displayName ?? null, deviceTypeId: relay.deviceType?.id ?? null, tagIds: relay.tags.map(tag => tag.id) }))">{{ t('common.save') }}</UButton><div class="sm:col-span-3 flex flex-wrap gap-2"><label v-for="tag in facilities.tags" :key="tag.id" class="check-row"><input type="checkbox" :checked="relay.tags.some(item => item.id === tag.id)" @change="($event.target as HTMLInputElement).checked ? relay.tags.push(tag) : relay.tags.splice(relay.tags.findIndex(item => item.id === tag.id), 1)">{{ tag.name }}</label></div></div></section>
        <form class="space-y-4" @submit.prevent="savePlacement"><div><h3 class="font-semibold">{{ t('admin.nodes.placement') }}</h3><p class="text-xs text-muted">{{ t('admin.nodes.placementDescription') }}</p></div><select v-model="selectedRelayId" class="form-control w-full"><option :value="null" disabled>{{ t('admin.nodes.chooseCapability') }}</option><option v-for="relay in selected.relays" :key="relay.id || relay.channel" :value="relay.id ?? null" :disabled="!relay.id">{{ relay.displayName || relay.discoveryName || relay.name }} · #{{ relay.channel }}{{ relay.id ? '' : ` (${t('admin.nodes.missingId')})` }}</option></select><div class="placement-editor"><div class="placement-preview"><span :style="{ left: `${placement.x}%`, top: `${placement.y}%`, width: `${placement.width * 2}px`, height: `${placement.height * 2}px`, transform: `translate(-50%, -50%) rotate(${rotation}deg) scale(${scale})` }"><UIcon name="i-lucide-radio" /></span></div><div class="grid gap-3"><label v-for="key in (['x','y','width','height'] as const)" :key="key" class="text-xs text-muted"><span class="flex justify-between"><b class="uppercase">{{ key }}</b><span class="tabular-nums">{{ placement[key] }}</span></span><input v-model.number="placement[key]" type="range" :min="key === 'width' || key === 'height' ? 1 : 0" :max="key === 'width' || key === 'height' ? 30 : 100" step="1" class="w-full accent-green-700"></label><label class="text-xs text-muted"><span class="flex justify-between"><b>ROTATION</b><span>{{ rotation }}</span></span><input v-model.number="rotation" type="range" min="-180" max="180" class="w-full accent-green-700"></label><label class="text-xs text-muted"><span class="flex justify-between"><b>SCALE</b><span>{{ scale }}</span></span><input v-model.number="scale" type="range" min="0.5" max="2" step="0.1" class="w-full accent-green-700"></label><label class="text-xs text-muted"><span class="field-label">{{ t('admin.folders.sortOrder') }}</span><UInput v-model.number="placement.sortOrder" type="number" class="w-full" /></label></div></div><UButton type="submit" :loading="busy" :disabled="!selectedRelay?.id || !selected.folderId">{{ t('common.saveChanges') }}</UButton></form>
      </div>
    </section>
  </div>
</template>
