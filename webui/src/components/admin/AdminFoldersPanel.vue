<script setup lang="ts">
import { ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import * as api from '@/api/admin'
import { useFacilitiesStore } from '@/stores/facilities'
import { localizedError } from '@/i18n/errors'
import type { FolderMutation, FolderTemplate } from '@/types/facility'

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
  if (!selectedId.value || !confirm(t('admin.folders.confirmDelete'))) return
  const id = selectedId.value
  selectedId.value = null
  void run(() => api.deleteFolder(id))
}
</script>

<template>
  <section class="admin-grid">
    <div class="admin-list">
      <div class="flex items-center justify-between px-3 py-2">
        <h2 class="font-semibold">{{ t('admin.folders.structure') }}</h2>
        <UButton size="xs" icon="i-lucide-plus" @click="selectedId = null">{{ t('common.new') }}</UButton>
      </div>
      <button v-for="folder in facilities.folders" :key="folder.id" type="button" class="admin-list-row" :class="{ active: selectedId === folder.id }" @click="selectedId = folder.id">
        <UIcon :name="folder.icon || 'i-lucide-folder'" />
        <span class="min-w-0 flex-1 truncate text-left">{{ folder.name }}</span>
        <span class="text-[10px] text-muted">{{ folder.templateType }}</span>
      </button>
    </div>
    <form class="space-y-5 p-4 sm:p-6" @submit.prevent="save">
      <div>
        <p class="section-kicker">{{ selectedId ? t('admin.folders.edit') : t('admin.folders.create') }}</p>
        <h2 class="flex items-center gap-2 text-xl font-semibold"><UIcon :name="icon" class="size-5 text-primary" />{{ name || t('admin.folders.untitled') }}</h2>
      </div>
      <UAlert v-if="error" color="error" variant="subtle" :description="error" />
      <UFormField :label="t('admin.folders.name')"><UInput v-model="name" class="w-full" required /></UFormField>
      <div class="grid gap-4 sm:grid-cols-2">
        <UFormField :label="t('admin.folders.parent')">
          <select v-model="parentId" class="form-control w-full"><option :value="null">{{ t('admin.folders.root') }}</option><option v-for="folder in facilities.folders.filter(item => item.id !== selectedId)" :key="folder.id" :value="folder.id">{{ folder.name }}</option></select>
        </UFormField>
        <UFormField :label="t('admin.folders.sortOrder')"><UInput v-model.number="sortOrder" type="number" class="w-full" /></UFormField>
      </div>
      <UFormField :label="t('admin.folders.icon')" :description="t('admin.folders.iconDescription')">
        <div class="grid grid-cols-6 gap-2 sm:grid-cols-12">
          <button v-for="option in icons" :key="option" type="button" class="icon-option" :class="{ active: icon === option }" :aria-label="option.replace('i-lucide-', '')" :title="option.replace('i-lucide-', '')" @click="icon = option"><UIcon :name="option" class="size-5" /></button>
        </div>
      </UFormField>
      <UFormField :label="t('admin.folders.template')" :description="t('admin.folders.templateDescription')">
        <div class="grid grid-cols-2 gap-2 sm:grid-cols-5">
          <button v-for="template in templates" :key="template" type="button" class="template-option" :class="{ active: templateType === template }" @click="templateType = template"><UIcon :name="templateIcons[template]" /><span>{{ template }}</span></button>
        </div>
      </UFormField>
      <div class="flex justify-between gap-2">
        <UButton v-if="selectedId" type="button" color="error" variant="ghost" @click="remove">{{ t('common.delete') }}</UButton>
        <UButton type="submit" :loading="busy" class="ml-auto">{{ t('common.save') }}</UButton>
      </div>
    </form>
  </section>
</template>
