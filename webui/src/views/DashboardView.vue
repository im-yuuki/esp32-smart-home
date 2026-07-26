<script setup lang="ts">
import { onMounted } from 'vue'
import RoomSection from '@/components/RoomSection.vue'
import { useNodesStore } from '@/stores/nodes'

const store = useNodesStore()

onMounted(() => {
  // WS onConnect also fetches; this covers REST-up-but-WS-still-connecting.
  if (!store.loaded && !store.loading) void store.fetchNodes()
})
</script>

<template>
  <UContainer class="py-6">
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
