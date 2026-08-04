<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'
const props = withDefaults(defineProps<{ variant?: 'default' | 'destructive'; class?: string }>(), { variant: 'default' })
const classes = computed(() => cn('relative w-full rounded-md border px-3 py-2 text-sm', props.variant === 'destructive' ? 'border-danger/40 bg-danger/10 text-danger' : 'border-border bg-muted text-foreground', props.class))
</script>
<template><div role="alert" :class="classes"><slot /></div></template>
