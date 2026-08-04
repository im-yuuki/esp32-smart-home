<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'
const props = withDefaults(defineProps<{ variant?: 'default' | 'secondary' | 'outline' | 'success' | 'warning' | 'destructive'; class?: string }>(), { variant: 'default' })
const classes = computed(() => cn('inline-flex items-center rounded-full border px-2 py-0.5 text-[10px] font-medium', props.variant === 'default' && 'border-foreground bg-foreground text-background', props.variant === 'secondary' && 'border-transparent bg-muted text-muted-foreground', props.variant === 'outline' && 'border-border text-foreground', props.variant === 'success' && 'border-success bg-success text-white', props.variant === 'warning' && 'border-warning bg-warning text-white', props.variant === 'destructive' && 'border-danger bg-danger text-white', props.class))
</script>
<template><span :class="classes"><slot /></span></template>
