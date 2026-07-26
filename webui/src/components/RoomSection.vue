<script setup lang="ts">
import type { NodeInfo } from '@/types/api'
import NodeCard from '@/components/NodeCard.vue'
import { roomLabel } from '@/utils/rooms'

defineProps<{ room: string; nodes: NodeInfo[] }>()
</script>

<template>
  <section class="mb-8">
    <h2 class="mb-3 text-lg font-semibold text-highlighted">{{ roomLabel(room) }}</h2>
    <!-- Mobile-first: single column at 375px, 2-3 columns on larger screens. -->
    <div class="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3">
      <NodeCard v-for="node in nodes" :key="node.nodeId" :node="node" />
    </div>
  </section>
</template>
