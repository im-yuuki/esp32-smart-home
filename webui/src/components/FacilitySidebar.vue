<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import FolderTree from './FolderTree.vue'
import { useFacilitiesStore, type FolderTreeNode } from '@/stores/facilities'
import { useAuthStore } from '@/stores/auth'
import Input from '@/components/ui/Input.vue'
import Skeleton from '@/components/ui/Skeleton.vue'
import Alert from '@/components/ui/Alert.vue'

const emit = defineEmits<{ navigate: [] }>()
const facilities = useFacilitiesStore()
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
  try { const saved = JSON.parse(localStorage.getItem(key) ?? '[]') as unknown; collapsedIds.value = new Set(Array.isArray(saved) ? saved.filter((id): id is string => typeof id === 'string') : []) } catch { collapsedIds.value = new Set() }
}, { immediate: true })
watch(() => facilities.folders.map((folder) => folder.id).join(','), () => {
  const valid = new Set(facilities.folders.map((folder) => folder.id))
  const next = new Set([...collapsedIds.value].filter((id) => valid.has(id)))
  if (next.size !== collapsedIds.value.size) { collapsedIds.value = next; saveTreeState() }
})
onMounted(() => { void facilities.load() })
</script>

<template>
  <aside class="flex h-full w-72 flex-col border-r border-[var(--app-border)] bg-[var(--app-surface-muted)]">
    <div class="border-b border-[var(--app-border)] p-3"><Input v-model="query" :placeholder="t('navigation.search')" :aria-label="t('navigation.search')" /></div>
    <nav class="min-h-0 flex-1 overflow-y-auto p-2" :aria-label="t('navigation.browse')">
      <p class="mb-2 px-2 text-[11px] font-semibold uppercase tracking-[0.14em] text-[var(--app-text-muted)]">{{ t('navigation.facilities') }}</p>
      <Skeleton v-if="facilities.loading && !facilities.loaded" class="h-40 w-full" />
      <Alert v-else-if="facilities.error" variant="destructive">{{ facilities.error }}</Alert>
      <FolderTree v-else :nodes="filteredTree" :expanded="expanded" @toggle="toggle" @navigate="emit('navigate')" />
    </nav>
  </aside>
</template>
