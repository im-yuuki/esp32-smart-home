<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import AdminFoldersPanel from '@/components/admin/AdminFoldersPanel.vue'
import AdminNodesPanel from '@/components/admin/AdminNodesPanel.vue'
import AdminAccessPanel from '@/components/admin/AdminAccessPanel.vue'
import { useFacilitiesStore } from '@/stores/facilities'

type Tab = 'folders' | 'nodes' | 'access'
const tab = ref<Tab>('folders')
const selectedFolderId = ref<string | null>(null)
const facilities = useFacilitiesStore()
const { t } = useI18n()
onMounted(async () => { await facilities.load(); selectedFolderId.value = facilities.folders[0]?.id ?? null })
</script>

<template>
  <div class="mx-auto max-w-[1500px] space-y-5 p-4 sm:p-6">
    <header><p class="section-kicker">{{ t('admin.eyebrow') }}</p><h1 class="text-3xl font-semibold tracking-tight">{{ t('admin.title') }}</h1><p class="mt-1 max-w-2xl text-sm text-muted">{{ t('admin.description') }}</p></header>
    <nav class="flex gap-1 overflow-x-auto border-b border-default" :aria-label="t('admin.title')"><button v-for="item in (['folders', 'nodes', 'access'] as Tab[])" :key="item" type="button" class="admin-tab" :class="{ active: tab === item }" @click="tab = item"><UIcon :name="item === 'folders' ? 'i-lucide-folder-tree' : item === 'nodes' ? 'i-lucide-cpu' : 'i-lucide-key-round'" />{{ t(`admin.tabs.${item}`) }}</button></nav>
    <USkeleton v-if="facilities.loading && !facilities.loaded" class="h-96 w-full" />
    <UAlert v-else-if="facilities.error" color="error" variant="subtle" :description="facilities.error" />
    <AdminFoldersPanel v-else-if="tab === 'folders'" v-model="selectedFolderId" />
    <AdminNodesPanel v-else-if="tab === 'nodes'" />
    <AdminAccessPanel v-else />
  </div>
</template>
