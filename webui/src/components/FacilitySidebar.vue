<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import FolderTree from './FolderTree.vue'
import { useFacilitiesStore, type FolderTreeNode } from '@/stores/facilities'
import { useNodesStore } from '@/stores/nodes'
import { useAuthStore } from '@/stores/auth'

defineProps<{ collapsed?: boolean }>()
const emit = defineEmits<{ navigate: [] }>()
const facilities = useFacilitiesStore()
const nodes = useNodesStore()
const auth = useAuthStore()
const { t } = useI18n()
const query = ref('')
const collapsedIds = ref(new Set<string>())

const normalize = (value: string) => value.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLocaleLowerCase()
const filteredTree = computed(() => {
  const needle = normalize(query.value.trim())
  if (!needle) return facilities.tree
  const filter = (items: FolderTreeNode[]): FolderTreeNode[] => items.flatMap((item) => {
    const children = filter(item.children)
    return normalize(`${item.name} ${item.templateType} ${item.id}`).includes(needle) || children.length ? [{ ...item, children }] : []
  })
  return filter(facilities.tree)
})
const nodeMatches = computed(() => {
  const needle = normalize(query.value.trim())
  if (!needle) return []
  return [...nodes.nodes.values()].filter((node) => normalize([
    node.displayName, node.discoveryName, node.nodeId, node.room,
    ...node.relays.flatMap((relay) => [relay.displayName, relay.name, relay.deviceType?.name, ...relay.tags.map((tag) => tag.name)]),
  ].filter(Boolean).join(' ')).includes(needle)).slice(0, 8)
})
const visibleTreeIds = (items: FolderTreeNode[]): string[] => items.flatMap((item) => [item.id, ...visibleTreeIds(item.children)])
const expanded = computed(() => {
  const ids = query.value.trim() ? visibleTreeIds(filteredTree.value) : facilities.folders.map((folder) => folder.id)
  return new Set(ids.filter((id) => !collapsedIds.value.has(id) || Boolean(query.value.trim())))
})

function storageKey() { return auth.user ? `facility-tree-collapsed:${auth.user.id}` : null }
function saveTreeState() { const key = storageKey(); if (key) localStorage.setItem(key, JSON.stringify([...collapsedIds.value])) }
function toggle(id: string) { const next = new Set(collapsedIds.value); next.has(id) ? next.delete(id) : next.add(id); collapsedIds.value = next; saveTreeState() }
watch(() => auth.user?.id, () => {
  const key = storageKey()
  if (!key) { collapsedIds.value = new Set(); return }
  try {
    const saved = JSON.parse(localStorage.getItem(key) ?? '[]') as unknown
    collapsedIds.value = new Set(Array.isArray(saved) ? saved.filter((id): id is string => typeof id === 'string') : [])
  } catch { collapsedIds.value = new Set() }
}, { immediate: true })
watch(() => facilities.folders.map((folder) => folder.id).join(','), () => {
  const valid = new Set(facilities.folders.map((folder) => folder.id))
  const next = new Set([...collapsedIds.value].filter((id) => valid.has(id)))
  if (next.size !== collapsedIds.value.size) { collapsedIds.value = next; saveTreeState() }
})
onMounted(() => { void facilities.load(); if (!nodes.loaded) void nodes.fetchNodes() })
</script>

<template>
  <aside class="flex h-full flex-col border-r border-default bg-muted/60" :class="collapsed ? 'w-16' : 'w-72'">
    <div v-if="!collapsed" class="border-b border-default p-3">
      <UInput v-model="query" icon="i-lucide-search" :placeholder="t('navigation.search')" :aria-label="t('navigation.search')" class="w-full" />
    </div>
    <nav class="min-h-0 flex-1 overflow-y-auto p-2" :aria-label="t('navigation.browse')">
      <template v-if="!collapsed">
        <p class="mb-2 px-2 text-[11px] font-semibold uppercase tracking-[0.14em] text-muted">{{ t('navigation.facilities') }}</p>
        <USkeleton v-if="facilities.loading && !facilities.loaded" class="h-40 w-full" />
        <UAlert v-else-if="facilities.error" color="error" variant="subtle" :description="facilities.error" />
        <FolderTree v-else :nodes="filteredTree" :expanded="expanded" @toggle="toggle" @navigate="emit('navigate')" />
        <div v-if="nodeMatches.length" class="mt-5 border-t border-default pt-3">
          <p class="mb-1 px-2 text-[11px] font-semibold uppercase tracking-[0.14em] text-muted">{{ t('navigation.devices') }}</p>
          <RouterLink v-for="node in nodeMatches" :key="node.nodeId" :to="`/nodes/${node.nodeId}`" class="flex items-center gap-2 rounded px-2 py-2 text-sm hover:bg-elevated" @click="emit('navigate')">
            <span class="size-2 rounded-full" :class="node.online ? 'bg-primary' : 'bg-neutral-400'" />
            <span class="min-w-0"><span class="block truncate font-medium">{{ node.displayName || node.discoveryName || node.nodeId }}</span><span class="block truncate text-xs text-muted">{{ node.nodeId }}</span></span>
          </RouterLink>
        </div>
      </template>
      <template v-else>
        <RouterLink v-for="item in facilities.tree" :key="item.id" :to="`/browse/${item.id}`" class="mb-1 grid size-11 place-items-center rounded text-muted hover:bg-elevated hover:text-primary" :title="item.name"><UIcon :name="item.icon || 'i-lucide-folder'" class="size-5" /></RouterLink>
      </template>
    </nav>
    <div v-if="!collapsed" class="space-y-1 border-t border-default p-2">
      <RouterLink v-if="auth.user?.systemAdmin" to="/admin" class="nav-link" @click="emit('navigate')"><UIcon name="i-lucide-settings-2" />{{ t('header.admin') }}</RouterLink>
    </div>
  </aside>
</template>
