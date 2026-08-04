<script setup lang="ts">
import { useRoute } from 'vue-router'
import type { FolderTreeNode } from '@/stores/facilities'
import AppIcon from '@/components/AppIcon.vue'

const props = defineProps<{ nodes: FolderTreeNode[]; expanded: Set<string>; depth?: number }>()
const emit = defineEmits<{ toggle: [id: string]; navigate: [] }>()
const route = useRoute()
</script>

<template>
  <ul class="space-y-0.5" :class="{ 'ml-3 border-l border-[var(--app-border)] pl-2': depth }" :aria-label="depth ? undefined : 'Folder tree'">
    <li v-for="node in nodes" :key="node.id">
      <div class="group flex items-center">
        <button v-if="node.children.length" type="button" class="grid size-7 shrink-0 place-items-center rounded text-[var(--app-text-muted)] hover:bg-[var(--app-surface-elevated)] focus-visible:outline-2 focus-visible:outline-[var(--app-accent)]" :aria-expanded="expanded.has(node.id)" @click="emit('toggle', node.id)">
          <AppIcon name="i-lucide-chevron-right" class="size-3.5 transition-transform" :class="{ 'rotate-90': expanded.has(node.id) }" />
        </button>
        <span v-else class="block size-7 shrink-0" />
        <RouterLink v-if="node.permissions.includes('NODE_VIEW')" :to="`/browse/${node.id}`" class="flex min-w-0 flex-1 items-center gap-2 rounded px-2 py-2 text-sm transition-colors hover:bg-[var(--app-surface-elevated)] focus-visible:outline-2 focus-visible:outline-[var(--app-accent)]" :class="route.params.folderId === node.id ? 'bg-[var(--app-accent-soft)] font-medium text-[var(--app-accent)]' : 'text-[var(--app-text)]'" @click="emit('navigate')">
          <AppIcon :name="node.icon || 'i-lucide-folder'" class="size-4 shrink-0" />
          <span class="truncate">{{ node.name }}</span>
        </RouterLink>
        <span v-else class="flex min-w-0 flex-1 items-center gap-2 px-2 py-2 text-sm text-[var(--app-text-muted)]">
          <AppIcon :name="node.icon || 'i-lucide-folder'" class="size-4 shrink-0" />
          <span class="truncate">{{ node.name }}</span>
        </span>
      </div>
      <FolderTree v-if="node.children.length && expanded.has(node.id)" :nodes="node.children" :expanded="expanded" :depth="(depth ?? 0) + 1" @toggle="emit('toggle', $event)" @navigate="emit('navigate')" />
    </li>
  </ul>
</template>
