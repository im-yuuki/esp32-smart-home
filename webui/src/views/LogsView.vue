<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { listAuditLogs } from '@/api/facilities'
import { localizedError } from '@/i18n/errors'
import { useFacilitiesStore } from '@/stores/facilities'
import type { AuditFilters, AuditLog } from '@/types/facility'
import Input from '@/components/ui/Input.vue'
import Button from '@/components/ui/Button.vue'
import Alert from '@/components/ui/Alert.vue'
import Skeleton from '@/components/ui/Skeleton.vue'
import Badge from '@/components/ui/Badge.vue'
import AppIcon from '@/components/AppIcon.vue'

const { locale, t } = useI18n()
const facilities = useFacilitiesStore()
const filters = ref<AuditFilters>({ page: 0, size: 24, includeDescendants: false })
const items = ref<AuditLog[]>([])
const total = ref(0)
const totalPages = ref(0)
const loading = ref(false)
const error = ref<string | null>(null)
const expanded = ref<string | null>(null)
const formatTime = (value: string) => new Intl.DateTimeFormat(locale.value, { dateStyle: 'short', timeStyle: 'medium' }).format(new Date(value))
const latest = computed(() => items.value[0])

async function load(reset = false) {
  if (loading.value) return
  if (reset) filters.value.page = 0
  loading.value = true; error.value = null
  try { const result = await listAuditLogs(filters.value); items.value = result.items; total.value = result.total; totalPages.value = result.totalPages } catch (cause) { error.value = localizedError(cause) } finally { loading.value = false }
}
function page(delta: number) { filters.value.page += delta; void load() }
onMounted(() => { void facilities.load(); void load() })
</script>

<template>
  <div class="mx-auto max-w-[1500px] space-y-5 p-4 sm:p-6">
    <header class="flex items-end justify-between gap-3 border-b border-border pb-4"><div><p class="section-kicker">{{ t('activity.label') }}</p><h1 class="text-2xl font-semibold tracking-tight">{{ t('activity.title') }}</h1></div><div class="font-mono text-xs text-muted-foreground">{{ total }} {{ t('logs.result') }}</div></header>
    <form class="grid gap-2 border-y border-border py-3 sm:grid-cols-2 lg:grid-cols-6" @submit.prevent="load(true)"><input v-model="filters.from" type="datetime-local" class="form-control" :aria-label="t('logs.from')"><input v-model="filters.to" type="datetime-local" class="form-control" :aria-label="t('logs.to')"><select v-model="filters.folderId" class="form-control"><option :value="undefined">{{ t('logs.allFolders') }}</option><option v-for="folder in facilities.folders" :key="folder.id" :value="folder.id">{{ folder.name }}</option></select><Input v-model="filters.action" :placeholder="t('logs.action')" /><Input v-model="filters.actor" :placeholder="t('logs.actor')" /><div class="flex gap-2"><Input v-model="filters.node" :placeholder="t('logs.node')" class="min-w-0 flex-1" /><Button type="submit" size="icon" variant="outline" :aria-label="t('logs.filter')"><AppIcon name="i-lucide-search" /></Button></div><label v-if="filters.folderId" class="check-row sm:col-span-2 lg:col-span-3"><input v-model="filters.includeDescendants" type="checkbox">{{ t('logs.includeDescendants') }}</label></form>
    <Alert v-if="error" variant="destructive">{{ error }}</Alert>
    <section class="overflow-hidden rounded-lg border border-border bg-card" aria-live="polite">
      <div v-if="loading && !items.length" class="space-y-2 p-4"><Skeleton v-for="index in 6" :key="index" class="h-8 w-full" /></div>
      <div v-else-if="!items.length" class="p-8 text-center text-sm text-muted-foreground">{{ t('logs.empty') }}</div>
      <template v-else><button v-for="item in items" :key="item.id" type="button" class="grid w-full grid-cols-[7rem_minmax(0,1fr)_auto] items-center gap-3 border-b border-border px-3 py-3 text-left hover:bg-muted sm:grid-cols-[10rem_minmax(0,1fr)_10rem_auto]" @click="expanded = expanded === item.id ? null : item.id"><time class="font-mono text-[10px] text-muted-foreground" :datetime="item.timestamp">{{ formatTime(item.timestamp) }}</time><span class="min-w-0"><strong class="block truncate font-mono text-xs">{{ item.action }}</strong><span class="block truncate text-xs text-muted-foreground">{{ item.nodeId || `${item.targetType} ${item.targetId ?? ''}` }}<template v-if="item.folderId"> · {{ facilities.folderById.get(item.folderId)?.name }}</template></span></span><span class="hidden truncate text-xs sm:block">{{ item.actor }}</span><Badge variant="outline">{{ item.result }}</Badge><div v-if="expanded === item.id" class="col-span-full border-t border-border pt-3 text-xs text-muted-foreground"><span v-if="item.batchId" class="font-mono">{{ t('logs.batch') }} {{ item.batchId }}</span><span v-if="item.correlationId" class="ml-3 font-mono">{{ item.correlationId }}</span></div></button></template>
    </section>
    <footer class="flex items-center justify-between text-xs text-muted-foreground"><span>{{ latest?.action ?? t('logs.empty') }}</span><div class="flex items-center gap-2"><Button size="icon" variant="ghost" :disabled="filters.page === 0" @click="page(-1)"><AppIcon name="i-lucide-chevron-left" /></Button><span class="font-mono">{{ filters.page + 1 }} / {{ Math.max(totalPages, 1) }}</span><Button size="icon" variant="ghost" :disabled="filters.page + 1 >= totalPages" @click="page(1)"><AppIcon name="i-lucide-chevron-right" /></Button></div></footer>
  </div>
</template>
