<script setup lang="ts">
import { ref } from 'vue'
import { useI18n } from 'vue-i18n'
import * as api from '@/api/facilities'
import { useFacilitiesStore } from '@/stores/facilities'
import { localizedError } from '@/i18n/errors'
import type { BulkActionPayload, BulkActionResult } from '@/types/facility'
import Button from '@/components/ui/Button.vue'
import Checkbox from '@/components/ui/Checkbox.vue'
import Alert from '@/components/ui/Alert.vue'
import AppIcon from '@/components/AppIcon.vue'
import { motion } from 'motion-v'

const props = defineProps<{ folderId: string }>()
const facilities = useFacilitiesStore()
const { t } = useI18n()
const open = ref(false)
const includeDescendants = ref(true)
const deviceTypeIds = ref<string[]>([])
const tagIds = ref<string[]>([])
const tagMatch = ref<'ANY' | 'ALL'>('ANY')
const state = ref<'ON' | 'OFF'>('OFF')
const preview = ref<BulkActionResult | null>(null)
const result = ref<BulkActionResult | null>(null)
const busy = ref(false)
const error = ref<string | null>(null)

function payload(idempotencyKey: string): BulkActionPayload { return { includeDescendants: includeDescendants.value, deviceTypeIds: deviceTypeIds.value, tagIds: tagIds.value, tagMatch: tagMatch.value, state: state.value, idempotencyKey } }
async function run(execute: boolean) { busy.value = true; error.value = null; try { const response = execute ? await api.executeBulkAction(props.folderId, payload(crypto.randomUUID())) : await api.previewBulkAction(props.folderId, payload('preview')); execute ? result.value = response : preview.value = response } catch (cause) { error.value = localizedError(cause) } finally { busy.value = false } }
</script>

<template>
  <div>
    <Button variant="outline" @click="open = !open"><AppIcon name="i-lucide-list-checks" />{{ t('bulk.title') }}</Button>
    <div v-if="open" class="fixed inset-0 z-50 bg-black/45" @keydown.esc="open = false"><button type="button" class="absolute inset-0" :aria-label="t('common.close')" @click="open = false" /><motion.section :initial="{ x: 32, opacity: 0 }" :animate="{ x: 0, opacity: 1 }" :transition="{ duration: 0.16 }" class="absolute inset-y-0 right-0 w-[min(28rem,94vw)] space-y-4 overflow-y-auto border-l border-border bg-card p-5 text-card-foreground shadow-2xl" role="dialog" aria-modal="true" :aria-label="t('bulk.title')">
      <div class="flex items-center justify-between"><h2 class="font-semibold">{{ t('bulk.title') }}</h2><Button variant="ghost" size="icon" :aria-label="t('common.close')" @click="open = false"><AppIcon name="i-lucide-x" /></Button></div>
      <label class="check-row"><Checkbox v-model="includeDescendants" binary />{{ t('bulk.descendants') }}</label>
      <div class="grid gap-4 md:grid-cols-2">
        <fieldset><legend class="field-label">{{ t('bulk.deviceTypes') }}</legend><label v-for="item in facilities.deviceTypes" :key="item.id" class="check-row"><Checkbox v-model="deviceTypeIds" :value="item.id" />{{ item.name }}</label></fieldset>
        <fieldset><legend class="field-label">{{ t('bulk.tags') }}</legend><div class="mb-2 flex gap-1"><button v-for="mode in (['ANY', 'ALL'] as const)" :key="mode" type="button" class="rounded border px-2 py-1 text-[10px]" :class="tagMatch === mode ? 'bg-foreground text-background' : 'border-border'" @click="tagMatch = mode">{{ mode }}</button></div><label v-for="tag in facilities.tags" :key="tag.id" class="check-row"><Checkbox v-model="tagIds" :value="tag.id" />{{ tag.name }}</label></fieldset>
      </div>
      <div><span class="field-label">{{ t('bulk.targetState') }}</span><div class="flex gap-1"><button v-for="target in (['ON', 'OFF'] as const)" :key="target" type="button" class="rounded border px-3 py-1 text-xs" :class="state === target ? 'bg-foreground text-background' : 'border-border'" @click="state = target">{{ target }}</button></div></div>
      <Alert v-if="error" variant="destructive">{{ error }}</Alert>
      <div v-if="preview" class="grid grid-cols-3 gap-2 bg-[var(--app-surface)] p-3 text-center text-sm"><div><strong class="block text-lg tabular-nums">{{ preview.dispatchable }}</strong>{{ t('bulk.eligible') }}</div><div><strong class="block text-lg tabular-nums">{{ preview.skipped }}</strong>{{ t('bulk.skippedLabel') }}</div><div><strong class="block text-lg tabular-nums">{{ preview.matched }}</strong>{{ t('bulk.total') }}</div></div>
      <div v-if="result" class="space-y-1 text-sm text-[var(--app-accent)]"><p>{{ t('bulk.queued', { count: result.dispatched }) }} <span v-if="result.skipped" class="text-[var(--app-text-muted)]">{{ t('bulk.skipped', { count: result.skipped }) }}</span></p><p v-if="result.failed" class="text-[var(--app-danger)]">{{ t('bulk.failed', { count: result.failed }) }}</p><p v-if="result.batchId" class="font-mono text-xs text-[var(--app-text-muted)]">{{ t('bulk.batch') }} {{ result.batchId }}</p></div>
      <div class="flex flex-wrap gap-2"><Button :loading="busy" variant="outline" @click="run(false)">{{ t('bulk.preview') }}</Button><Button :loading="busy" :disabled="!preview" @click="run(true)">{{ t('bulk.confirm') }}</Button></div>
    </motion.section></div>
  </div>
</template>
