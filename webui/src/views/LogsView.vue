<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { listAuditLogs } from '@/api/facilities'
import { useFacilitiesStore } from '@/stores/facilities'
import { localizedError } from '@/i18n/errors'
import type { AuditFilters, AuditLog } from '@/types/facility'

const { locale, t } = useI18n()
const facilities = useFacilitiesStore()
const filters = ref<AuditFilters>({ page: 0, size: 10, includeDescendants: false })
const items = ref<AuditLog[]>([])
const total = ref(0)
const totalPages = ref(0)
const loading = ref(false)
const error = ref<string | null>(null)
async function load(reset = false) { if (reset) filters.value.page = 0; loading.value = true; error.value = null; try { const result = await listAuditLogs(filters.value); items.value = result.items; total.value = result.total; totalPages.value = result.totalPages } catch (cause) { error.value = localizedError(cause) } finally { loading.value = false } }
function page(delta: number) { filters.value.page += delta; void load() }
onMounted(async () => { await facilities.load(); await load() })
</script>

<template>
  <div class="mx-auto max-w-[1500px] space-y-5 p-4 sm:p-6">
    <header><p class="text-xs font-semibold uppercase tracking-[0.15em] text-primary">{{ t('logs.eyebrow') }}</p><h1 class="text-3xl font-semibold tracking-tight">{{ t('logs.title') }}</h1></header>
    <form class="grid gap-3 border-y border-default bg-muted/50 py-4 sm:grid-cols-2 xl:grid-cols-6" @submit.prevent="load(true)">
      <input v-model="filters.from" type="datetime-local" class="form-control" :aria-label="t('logs.from')"><input v-model="filters.to" type="datetime-local" class="form-control" :aria-label="t('logs.to')">
      <select v-model="filters.folderId" class="form-control" :aria-label="t('logs.folder')"><option :value="undefined">{{ t('logs.allFolders') }}</option><option v-for="folder in facilities.folders" :key="folder.id" :value="folder.id">{{ folder.name }}</option></select>
      <UInput v-model="filters.action" :placeholder="t('logs.action')" /><UInput v-model="filters.actor" :placeholder="t('logs.actor')" /><UInput v-model="filters.node" :placeholder="t('logs.node')" />
      <label v-if="filters.folderId" class="check-row sm:col-span-2 xl:col-span-3"><input v-model="filters.includeDescendants" type="checkbox">{{ t('logs.includeDescendants') }}</label>
      <UButton type="submit" icon="i-lucide-search" class="xl:col-start-6">{{ t('logs.filter') }}</UButton>
    </form>
    <UAlert v-if="error" color="error" variant="subtle" :description="error" />
    <div class="overflow-x-auto border-b border-default">
      <table class="w-full min-w-[850px] text-left text-sm"><thead class="border-b border-default text-xs uppercase tracking-wider text-muted"><tr><th class="py-3 pr-4">{{ t('logs.time') }}</th><th class="px-4 py-3">{{ t('logs.action') }}</th><th class="px-4 py-3">{{ t('logs.actor') }}</th><th class="px-4 py-3">{{ t('logs.target') }}</th><th class="px-4 py-3">{{ t('logs.trace') }}</th><th class="py-3 pl-4">{{ t('logs.result') }}</th></tr></thead>
        <tbody><tr v-if="loading" v-for="i in 6" :key="i" class="border-b border-default"><td v-for="j in 6" :key="j" class="p-4"><USkeleton class="h-4 w-full" /></td></tr><tr v-else v-for="item in items" :key="item.id" class="border-b border-default hover:bg-muted/60"><td class="whitespace-nowrap py-4 pr-4 tabular-nums">{{ new Intl.DateTimeFormat(locale, { dateStyle: 'short', timeStyle: 'medium' }).format(new Date(item.timestamp)) }}</td><td class="px-4 py-4 font-medium">{{ item.action }}</td><td class="px-4 py-4"><span class="block">{{ item.actor }}</span><span v-if="item.actorUserId" class="text-xs text-muted">#{{ item.actorUserId }}</span></td><td class="px-4 py-4"><span class="block">{{ item.nodeId || `${item.targetType} ${item.targetId ?? ''}` }}</span><span class="text-xs text-muted">{{ facilities.folderById.get(item.folderId || '')?.name }}</span></td><td class="px-4 py-4 font-mono text-xs"><span v-if="item.batchId" class="block">{{ t('logs.batch') }} {{ item.batchId }}</span><span v-if="item.correlationId" class="block text-muted">{{ item.correlationId }}</span></td><td class="py-4 pl-4"><UBadge :color="item.result === 'SUCCESS' ? 'primary' : 'neutral'" variant="subtle">{{ item.result }}</UBadge></td></tr><tr v-if="!loading && !items.length"><td colspan="6" class="py-12 text-center text-muted">{{ t('logs.empty') }}</td></tr></tbody>
      </table>
    </div>
    <footer class="flex items-center justify-between text-sm text-muted"><span>{{ t('logs.total', { count: total }) }}</span><div class="flex items-center gap-2"><UButton color="neutral" variant="outline" icon="i-lucide-chevron-left" :disabled="filters.page === 0" @click="page(-1)" /><span class="tabular-nums">{{ filters.page + 1 }} / {{ Math.max(totalPages, 1) }}</span><UButton color="neutral" variant="outline" icon="i-lucide-chevron-right" :disabled="filters.page + 1 >= totalPages" @click="page(1)" /></div></footer>
  </div>
</template>
