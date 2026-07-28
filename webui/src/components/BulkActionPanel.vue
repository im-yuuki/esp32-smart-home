<script setup lang="ts">
import { ref } from 'vue'
import { useI18n } from 'vue-i18n'
import * as api from '@/api/facilities'
import { useFacilitiesStore } from '@/stores/facilities'
import { localizedError } from '@/i18n/errors'
import type { BulkActionPayload, BulkActionResult } from '@/types/facility'

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
    <UButton color="neutral" variant="soft" icon="i-lucide-list-checks" @click="open = !open">{{ t('bulk.title') }}</UButton>
    <section v-if="open" class="mt-3 space-y-4 border-l-2 border-primary bg-muted p-4" :aria-label="t('bulk.title')">
      <div class="flex items-center justify-between"><h2 class="font-semibold">{{ t('bulk.title') }}</h2><UButton color="neutral" variant="ghost" icon="i-lucide-x" :aria-label="t('common.close')" @click="open = false" /></div>
      <label class="check-row"><input v-model="includeDescendants" type="checkbox">{{ t('bulk.descendants') }}</label>
      <div class="grid gap-4 md:grid-cols-2">
        <fieldset><legend class="field-label">{{ t('bulk.deviceTypes') }}</legend><label v-for="item in facilities.deviceTypes" :key="item.id" class="check-row"><input v-model="deviceTypeIds" type="checkbox" :value="item.id">{{ item.name }}</label></fieldset>
        <fieldset><legend class="field-label">{{ t('bulk.tags') }}</legend><div class="mb-2 flex gap-2"><button v-for="mode in (['ANY', 'ALL'] as const)" :key="mode" type="button" class="segmented" :class="{ active: tagMatch === mode }" @click="tagMatch = mode">{{ mode }}</button></div><label v-for="tag in facilities.tags" :key="tag.id" class="check-row"><input v-model="tagIds" type="checkbox" :value="tag.id">{{ tag.name }}</label></fieldset>
      </div>
      <div><span class="field-label">{{ t('bulk.targetState') }}</span><div class="flex gap-2"><button v-for="target in (['ON', 'OFF'] as const)" :key="target" type="button" class="segmented" :class="{ active: state === target }" @click="state = target">{{ target }}</button></div></div>
      <UAlert v-if="error" color="error" variant="subtle" :description="error" />
      <div v-if="preview" class="grid grid-cols-3 gap-2 bg-default p-3 text-center text-sm"><div><strong class="block text-lg tabular-nums">{{ preview.dispatchable }}</strong>{{ t('bulk.eligible') }}</div><div><strong class="block text-lg tabular-nums">{{ preview.skipped }}</strong>{{ t('bulk.skippedLabel') }}</div><div><strong class="block text-lg tabular-nums">{{ preview.matched }}</strong>{{ t('bulk.total') }}</div></div>
      <div v-if="result" class="space-y-1 text-sm text-primary"><p>{{ t('bulk.queued', { count: result.dispatched }) }} <span v-if="result.skipped" class="text-muted">{{ t('bulk.skipped', { count: result.skipped }) }}</span></p><p v-if="result.failed" class="text-error">{{ t('bulk.failed', { count: result.failed }) }}</p><p v-if="result.batchId" class="font-mono text-xs text-muted">{{ t('bulk.batch') }} {{ result.batchId }}</p></div>
      <div class="flex flex-wrap gap-2"><UButton :loading="busy" color="neutral" variant="outline" @click="run(false)">{{ t('bulk.preview') }}</UButton><UButton :loading="busy" :disabled="!preview" @click="run(true)">{{ t('bulk.confirm') }}</UButton></div>
    </section>
  </div>
</template>
