<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'
const props = defineProps<{ class?: string }>()
const classes = computed(() => cn('text-xs font-medium leading-none text-foreground', props.class))
</script>
<template><label :class="classes"><slot /></label></template>
