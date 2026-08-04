<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { listAuditLogs } from '@/api/facilities'
import { localizedError } from '@/i18n/errors'
import { useFacilitiesStore } from '@/stores/facilities'
import type { AuditFilters, AuditLog } from '@/types/facility'
import Button from '@/components/ui/Button.vue'
import Input from '@/components/ui/Input.vue'
import Alert from '@/components/ui/Alert.vue'
import Skeleton from '@/components/ui/Skeleton.vue'
import Badge from '@/components/ui/Badge.vue'
import AppIcon from '@/components/AppIcon.vue'

const { locale, t } = useI18n()
const facilities = useFacilitiesStore()
const filters = ref<AuditFilters>({ page: 0, size: 12, includeDescendants: false })
const items = ref<AuditLog[]>([])
const total = ref(0)
const totalPages = ref(0)
const loading = ref(false)
const error = ref<string | null>(null)
const open = ref(false)
const latest = computed(() => items.value[0] ?? null)
let pollTimer: number | undefined

function formatTime(value: string, compact = false) {
  return new Intl.DateTimeFormat(locale.value, compact
    ? { hour: '2-digit', minute: '2-digit', second: '2-digit' }
    : { dateStyle: 'short', timeStyle: 'medium' }).format(new Date(value))
}

function resultVariant(result: string): 'success' | 'warning' | 'destructive' {
  if (result === 'SUCCESS') return 'success'
  if (result === 'PENDING') return 'warning'
  return 'destructive'
}

async function load(reset = false) {
  if (loading.value) return
  if (reset) filters.value.page = 0
  loading.value = true
  error.value = null
  try {
    const result = await listAuditLogs(filters.value)
    items.value = result.items
    total.value = result.total
    totalPages.value = result.totalPages
  } catch (cause) { error.value = localizedError(cause) }
  finally { loading.value = false }
}

function page(delta: number) { filters.value.page += delta; void load() }
function toggle() { open.value = !open.value; if (open.value) void load() }

onMounted(() => {
  void facilities.load()
  void load()
  pollTimer = window.setInterval(() => { if (!open.value && !document.hidden) void load() }, 30_000)
})
onBeforeUnmount(() => { if (pollTimer) window.clearInterval(pollTimer) })
</script>

<template>
  <section class="activity-log-bar" :class="open ? 'h-[min(62dvh,34rem)]' : 'h-12'" :aria-label="t('activity.title')">
      <div v-if="open" class="flex min-h-0 flex-1 flex-col bg-[var(--app-surface)]">
        <header class="flex items-center justify-between border-b border-[var(--app-border)] px-3 py-2 sm:px-5">
        <div class="min-w-0">
          <p class="section-kicker mb-0">{{ t('activity.eyebrow') }}</p>
          <h2 class="truncate text-base font-semibold sm:text-lg">{{ t('activity.title') }} <span class="font-normal text-[var(--app-text-muted)]">· {{ total }}</span></h2>
        </div>
        <div class="flex items-center gap-1">
          <Button variant="ghost" size="icon" :loading="loading" :aria-label="t('activity.refresh')" @click="load()"><AppIcon name="i-lucide-refresh-cw" /></Button>
          <Button variant="ghost" size="icon" :aria-label="t('activity.collapse')" @click="open = false"><AppIcon name="i-lucide-chevron-down" /></Button>
        </div>
      </header>
        <form class="grid grid-cols-2 gap-2 border-b border-[var(--app-border)] bg-[var(--app-surface-muted)] p-3 lg:grid-cols-6" @submit.prevent="load(true)">
        <input v-model="filters.from" type="datetime-local" class="form-control hidden lg:block" :aria-label="t('logs.from')">
        <input v-model="filters.to" type="datetime-local" class="form-control hidden lg:block" :aria-label="t('logs.to')">
        <select v-model="filters.folderId" class="form-control"><option :value="undefined">{{ t('logs.allFolders') }}</option><option v-for="folder in facilities.folders" :key="folder.id" :value="folder.id">{{ folder.name }}</option></select>
        <Input v-model="filters.action" :placeholder="t('logs.action')" />
        <Input v-model="filters.actor" :placeholder="t('logs.actor')" />
        <div class="flex gap-2"><Input v-model="filters.node" :placeholder="t('logs.node')" class="min-w-0 flex-1" /><Button type="submit" variant="ghost" size="icon" :aria-label="t('logs.filter')"><AppIcon name="i-lucide-search" /></Button></div>
        <label v-if="filters.folderId" class="check-row sm:col-span-2 lg:col-span-3"><input v-model="filters.includeDescendants" type="checkbox">{{ t('logs.includeDescendants') }}</label>
      </form>
      <Alert v-if="error" variant="destructive" class="m-3">{{ error }}</Alert>
      <div class="min-h-0 flex-1 overflow-y-auto" aria-live="polite">
        <div v-if="loading && !items.length" class="space-y-px bg-[var(--app-surface)]"><div v-for="index in 5" :key="index" class="grid grid-cols-[7rem_1fr] gap-4 border-b border-[var(--app-border)] p-3 sm:grid-cols-[10rem_1fr_10rem_8rem]"><Skeleton class="h-4" /><Skeleton class="h-4" /><Skeleton class="hidden h-4 sm:block" /><Skeleton class="hidden h-4 sm:block" /></div></div>
        <div v-else-if="!items.length" class="grid h-full min-h-28 place-items-center px-4 text-sm text-[var(--app-text-muted)]">{{ t('logs.empty') }}</div>
        <article v-for="item in items" v-else :key="item.id" class="activity-row">
           <time class="text-[11px] text-[var(--app-text-muted)]" :datetime="item.timestamp">{{ formatTime(item.timestamp) }}</time>
          <div class="min-w-0">
            <p class="truncate font-mono text-xs font-semibold sm:text-sm">{{ item.action }}</p>
            <p class="truncate text-xs text-[var(--app-text-muted)]">{{ item.nodeId || `${item.targetType} ${item.targetId ?? ''}` }}<span v-if="item.folderId"> · {{ facilities.folderById.get(item.folderId)?.name }}</span></p>
          </div>
           <div class="hidden min-w-0 sm:block"><p class="truncate text-sm">{{ item.actor }}</p><p v-if="item.batchId" class="truncate text-[10px] text-[var(--app-text-muted)]">{{ t('logs.batch') }} {{ item.batchId }}</p></div>
           <Badge :variant="resultVariant(item.result)" class="min-w-16 justify-center text-center uppercase">{{ item.result }}</Badge>
        </article>
      </div>
        <footer class="flex items-center justify-between border-t border-[var(--app-border)] px-3 py-2 text-xs text-[var(--app-text-muted)] sm:px-5">
        <span>{{ t('logs.total', { count: total }) }}</span>
         <div class="flex items-center gap-2"><Button size="icon" variant="ghost" :disabled="filters.page === 0" @click="page(-1)"><AppIcon name="i-lucide-chevron-left" /></Button><span class="tabular-nums">{{ filters.page + 1 }} / {{ Math.max(totalPages, 1) }}</span><Button size="icon" variant="ghost" :disabled="filters.page + 1 >= totalPages" @click="page(1)"><AppIcon name="i-lucide-chevron-right" /></Button></div>
      </footer>
    </div>
    <button type="button" class="activity-console" :aria-expanded="open" @click="toggle">
       <span class="flex shrink-0 items-center gap-2 text-[10px] font-semibold uppercase tracking-[0.16em] text-foreground"><span class="size-1.5 rounded-full bg-foreground" />{{ t('activity.label') }}</span>
       <span v-if="latest" class="min-w-0 flex-1 truncate text-left text-xs text-muted-foreground"><time class="mr-2 text-muted-foreground">{{ formatTime(latest.timestamp, true) }}</time><strong class="font-mono font-medium text-foreground">{{ latest.action }}</strong><span class="hidden sm:inline"> · {{ latest.actor }} · {{ latest.nodeId || latest.targetType }}</span></span>
      <span v-else class="min-w-0 flex-1 truncate text-left text-xs text-muted-foreground">{{ loading ? t('activity.loading') : t('logs.empty') }}</span>
      <AppIcon :name="open ? 'i-lucide-chevron-down' : 'i-lucide-chevron-up'" class="size-4 shrink-0 text-muted-foreground" />
    </button>
  </section>
</template>
