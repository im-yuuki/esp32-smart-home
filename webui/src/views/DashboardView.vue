<script setup lang="ts">
import { onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import RoomSection from '@/components/RoomSection.vue'
import { useNodesStore } from '@/stores/nodes'
import { useAuthStore } from '@/stores/auth'

const store = useNodesStore()
const auth = useAuthStore()
const { t } = useI18n()

onMounted(() => {
  // WS onConnect also fetches; this covers REST-up-but-WS-still-connecting.
  if (!store.loaded && !store.loading) void store.fetchNodes()
})
</script>

<template>
  <UContainer class="py-6">
    <div v-if="auth.user?.groups.length" class="mb-5 flex items-center justify-between gap-4">
      <div>
        <h1 class="text-xl font-semibold text-highlighted">{{ t('dashboard.title') }}</h1>
        <p class="text-sm text-muted">{{ t('dashboard.description') }}</p>
      </div>
      <select v-model="store.selectedGroupId" class="rounded-md border border-default bg-default px-3 py-2 text-sm text-default">
        <option :value="null">{{ t('dashboard.allGroups') }}</option>
        <option v-for="group in auth.user.groups" :key="group.id" :value="group.id">{{ group.name }}</option>
      </select>
    </div>
    <!-- Initial load: skeleton cards -->
    <div v-if="!store.loaded && store.loading" class="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3">
      <USkeleton v-for="i in 6" :key="i" class="h-44 w-full rounded-lg" />
    </div>

    <!-- Initial load failed -->
    <div v-else-if="!store.loaded && store.loadError" class="space-y-4">
      <UAlert
        color="error"
        variant="subtle"
        icon="i-lucide-circle-alert"
        :title="t('dashboard.loadFailed')"
        :description="store.loadError"
      />
      <UButton icon="i-lucide-refresh-cw" @click="store.fetchNodes()">{{ t('common.retry') }}</UButton>
    </div>

    <template v-else>
      <UAlert
        v-if="store.nodesByRoom.length === 0"
        color="neutral"
        variant="subtle"
        icon="i-lucide-search"
        :title="t('dashboard.emptyTitle')"
        :description="t('dashboard.emptyDescription')"
      />
      <RoomSection
        v-for="[room, nodes] in store.nodesByRoom"
        :key="room"
        :room="room"
        :nodes="nodes"
      />
    </template>
  </UContainer>
</template>
