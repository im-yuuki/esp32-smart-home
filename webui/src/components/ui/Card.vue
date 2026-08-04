<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'
const props = defineProps<{ class?: string }>()
const classes = computed(() => cn('rounded-lg border border-border bg-card text-card-foreground', props.class))
</script>
<template><section :class="classes"><slot /></section></template>
