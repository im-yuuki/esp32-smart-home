<script setup lang="ts">
import { onMounted } from 'vue'
import RoomSection from '@/components/RoomSection.vue'
import { useNodesStore } from '@/stores/nodes'
import { useAuthStore } from '@/stores/auth'

const store = useNodesStore()
const auth = useAuthStore()

onMounted(() => {
  // WS onConnect also fetches; this covers REST-up-but-WS-still-connecting.
  if (!store.loaded && !store.loading) void store.fetchNodes()
})
</script>

<template>
  <UContainer class="py-6">
    <div v-if="auth.user?.groups.length" class="mb-5 flex items-center justify-between gap-4">
      <div>
        <h1 class="text-xl font-semibold text-highlighted">Nodes</h1>
        <p class="text-sm text-muted">Access is combined across your assigned groups.</p>
      </div>
      <select v-model="store.selectedGroupId" class="rounded-md border border-default bg-default px-3 py-2 text-sm text-default">
        <option :value="null">All groups</option>
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
        title="Failed to load nodes"
        :description="store.loadError"
      />
      <UButton icon="i-lucide-refresh-cw" @click="store.fetchNodes()">Retry</UButton>
    </div>

    <template v-else>
      <UAlert
        v-if="store.nodesByRoom.length === 0"
        color="neutral"
        variant="subtle"
        icon="i-lucide-search"
        title="No nodes discovered yet"
        description="Nodes appear here automatically once they publish their MQTT discovery message."
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
