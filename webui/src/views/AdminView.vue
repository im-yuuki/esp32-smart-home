<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import AdminFoldersPanel from '@/components/admin/AdminFoldersPanel.vue'
import AdminNodesPanel from '@/components/admin/AdminNodesPanel.vue'
import AdminAccessPanel from '@/components/admin/AdminAccessPanel.vue'
import { useFacilitiesStore } from '@/stores/facilities'
import Skeleton from '@/components/ui/Skeleton.vue'
import Alert from '@/components/ui/Alert.vue'

type Tab = 'folders' | 'nodes' | 'access'
const tab = ref<Tab>('folders')
const selectedFolderId = ref<string | null>(null)
const facilities = useFacilitiesStore()
const { t } = useI18n()
onMounted(async () => { await facilities.load(); selectedFolderId.value = facilities.folders[0]?.id ?? null })
</script>

<template>
  <div class="mx-auto max-w-[1500px] space-y-5 p-4 sm:p-6">
    <header class="flex items-center justify-between gap-3 border-b border-[var(--app-border)] pb-4"><div><p class="section-kicker">{{ t('admin.eyebrow') }}</p><h1 class="text-2xl font-semibold tracking-tight">{{ t('admin.title') }}</h1></div></header>
    <Skeleton v-if="facilities.loading && !facilities.loaded" class="h-96 w-full" />
    <Alert v-else-if="facilities.error" variant="destructive">{{ facilities.error }}</Alert>
    <div v-else class="space-y-5">
      <nav class="flex gap-1 overflow-x-auto border-b border-border" role="tablist" :aria-label="t('admin.title')"><button v-for="item in (['folders', 'nodes', 'access'] as Tab[])" :key="item" type="button" role="tab" :aria-selected="tab === item" class="border-b-2 border-transparent px-3 py-2 text-xs font-medium text-muted-foreground" :class="tab === item ? 'border-foreground text-foreground' : ''" @click="tab = item">{{ t(`admin.tabs.${item}`) }}</button></nav>
      <AdminFoldersPanel v-if="tab === 'folders'" v-model="selectedFolderId" />
      <AdminNodesPanel v-else-if="tab === 'nodes'" />
      <AdminAccessPanel v-else />
    </div>
  </div>
</template>
