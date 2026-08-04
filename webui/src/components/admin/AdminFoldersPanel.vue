<script setup lang="ts">
import { ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import * as api from '@/api/admin'
import { useFacilitiesStore } from '@/stores/facilities'
import { localizedError } from '@/i18n/errors'
import type { FolderMutation, FolderTemplate } from '@/types/facility'
import Button from '@/components/ui/Button.vue'
import Input from '@/components/ui/Input.vue'
import Alert from '@/components/ui/Alert.vue'
import AppIcon from '@/components/AppIcon.vue'

const facilities = useFacilitiesStore()
const { t } = useI18n()
const selectedId = defineModel<string | null>({ required: true })
const name = ref('')
const icon = ref('i-lucide-folder')
const parentId = ref<string | null>(null)
const templateType = ref<FolderTemplate>('ROOM')
const templateConfig = ref<Record<string, unknown>>({})
const sortOrder = ref(0)
const busy = ref(false)
const error = ref<string | null>(null)
const confirmOpen = ref(false)
const pendingDelete = ref<string | null>(null)
const templates: FolderTemplate[] = ['OUTDOOR', 'BUILDING', 'FLOOR', 'CORRIDOR', 'ROOM']
const icons = [
  'i-lucide-folder', 'i-lucide-map-pin', 'i-lucide-trees', 'i-lucide-building-2',
  'i-lucide-house', 'i-lucide-layers-3', 'i-lucide-route', 'i-lucide-door-open',
  'i-lucide-sofa', 'i-lucide-bed-double', 'i-lucide-cooking-pot', 'i-lucide-warehouse',
]
const templateIcons: Record<FolderTemplate, string> = {
  OUTDOOR: 'i-lucide-square-dashed', BUILDING: 'i-lucide-panels-top-left',
  FLOOR: 'i-lucide-layout-template', CORRIDOR: 'i-lucide-panel-top', ROOM: 'i-lucide-square',
}

watch(selectedId, () => {
  const folder = selectedId.value ? facilities.folderById.get(selectedId.value) : null
  name.value = folder?.name ?? ''
  icon.value = folder?.icon ?? 'i-lucide-folder'
  parentId.value = folder?.parentId ?? null
  templateType.value = folder?.templateType ?? 'ROOM'
  templateConfig.value = JSON.parse(JSON.stringify(folder?.templateConfig ?? {})) as Record<string, unknown>
  sortOrder.value = folder?.sortOrder ?? 0
}, { immediate: true })

async function run(action: () => Promise<void>) {
  busy.value = true
  error.value = null
  try { await action(); await facilities.load(true) }
  catch (cause) { error.value = localizedError(cause) }
  finally { busy.value = false }
}

function save() {
  const payload: FolderMutation = {
    name: name.value.trim(), icon: icon.value, parentId: parentId.value,
    templateType: templateType.value, templateConfig: templateConfig.value, sortOrder: sortOrder.value,
  }
  void run(async () => {
    if (!selectedId.value) {
      const created = await api.createFolder(payload)
      selectedId.value = created.id
      return
    }
    const existing = facilities.folderById.get(selectedId.value)
    if (existing?.parentId !== parentId.value) await api.moveFolder(selectedId.value, parentId.value)
    await api.updateFolder(selectedId.value, payload)
  })
}

function remove() {
  if (!selectedId.value) return
  pendingDelete.value = selectedId.value
  confirmOpen.value = true
}

function acceptRemove() {
  const id = pendingDelete.value
  confirmOpen.value = false
  pendingDelete.value = null
  if (id) { selectedId.value = null; void run(() => api.deleteFolder(id)) }
}
</script>

<template>
  <section class="admin-grid">
    <div class="admin-list">
      <div class="flex items-center justify-between px-3 py-2">
        <h2 class="font-semibold">{{ t('admin.folders.structure') }}</h2>
        <Button size="icon" variant="ghost" @click="selectedId = null"><AppIcon name="i-lucide-plus" /></Button>
      </div>
      <button v-for="folder in facilities.folders" :key="folder.id" type="button" class="admin-list-row" :class="{ active: selectedId === folder.id }" @click="selectedId = folder.id">
        <AppIcon :name="folder.icon || 'i-lucide-folder'" />
        <span class="min-w-0 flex-1 truncate text-left">{{ folder.name }}</span>
        <span class="text-[10px] text-[var(--app-text-muted)]">{{ folder.templateType }}</span>
      </button>
    </div>
    <form class="space-y-5 p-4 sm:p-6" @submit.prevent="save">
      <div>
        <p class="section-kicker">{{ selectedId ? t('admin.folders.edit') : t('admin.folders.create') }}</p>
        <h2 class="flex items-center gap-2 text-xl font-semibold"><AppIcon :name="icon" class="size-5 text-[var(--app-accent)]" />{{ name || t('admin.folders.untitled') }}</h2>
      </div>
      <Alert v-if="error" variant="destructive">{{ error }}</Alert>
      <div class="space-y-1"><label for="folder-name" class="field-label">{{ t('admin.folders.name') }}</label><Input id="folder-name" v-model="name" required /></div>
      <div class="grid gap-4 sm:grid-cols-2">
        <div class="space-y-1"><label class="field-label">{{ t('admin.folders.parent') }}</label><select v-model="parentId" class="form-control"><option :value="null">{{ t('admin.folders.root') }}</option><option v-for="folder in facilities.folders.filter(item => item.id !== selectedId)" :key="folder.id" :value="folder.id">{{ folder.name }}</option></select></div>
        <div class="space-y-1"><label class="field-label">{{ t('admin.folders.sortOrder') }}</label><Input v-model.number="sortOrder" type="number" /></div>
      </div>
      <div>
        <label class="field-label">{{ t('admin.folders.icon') }}</label>
        <div class="grid grid-cols-6 gap-2 sm:grid-cols-12">
          <button v-for="option in icons" :key="option" type="button" class="icon-option" :class="{ active: icon === option }" :aria-label="option.replace('i-lucide-', '')" :title="option.replace('i-lucide-', '')" @click="icon = option"><AppIcon :name="option" class="size-5" /></button>
        </div>
      </div>
      <div>
        <label class="field-label">{{ t('admin.folders.template') }}</label>
        <div class="grid grid-cols-2 gap-2 sm:grid-cols-5">
          <button v-for="template in templates" :key="template" type="button" class="template-option" :class="{ active: templateType === template }" @click="templateType = template"><AppIcon :name="templateIcons[template]" /><span>{{ template }}</span></button>
        </div>
      </div>
      <div class="flex justify-between gap-2">
        <Button v-if="selectedId" type="button" variant="destructive" @click="remove">{{ t('common.delete') }}</Button>
        <Button type="submit" :loading="busy" class="ml-auto">{{ t('common.save') }}</Button>
      </div>
    </form>
    <div v-if="confirmOpen" class="fixed inset-0 z-50 grid place-items-center bg-black/50 p-4" role="dialog" aria-modal="true" :aria-label="t('common.delete')"><div class="w-full max-w-sm rounded-lg border border-border bg-card p-5 shadow-2xl"><h2 class="font-semibold">{{ t('common.delete') }}</h2><p class="mt-2 text-sm text-muted-foreground">{{ t('admin.folders.confirmDelete') }}</p><div class="mt-5 flex justify-end gap-2"><Button variant="ghost" @click="confirmOpen = false">{{ t('common.close') }}</Button><Button variant="destructive" @click="acceptRemove">{{ t('common.delete') }}</Button></div></div></div>
  </section>
</template>
