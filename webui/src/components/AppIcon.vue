<script setup lang="ts">
import { Icon, addCollection } from '@iconify/vue'
import { icons } from '@iconify-json/lucide'

addCollection(icons)

const props = defineProps<{ name: string }>()

function iconName(name: string): string {
  return name.startsWith('i-lucide-') ? `lucide:${name.slice(9)}` : name
}
</script>

<template>
  <Icon :icon="iconName(props.name)" aria-hidden="true" />
</template>
