<script setup lang="ts">
import { computed, useAttrs } from 'vue'
import { cn } from '@/lib/utils'

defineOptions({ inheritAttrs: false })
const props = defineProps<{ class?: string }>()
const attrs = useAttrs()
const classes = computed(() => cn('flex h-9 w-full rounded-md border border-border bg-background px-3 py-1 text-sm shadow-none outline-none transition-colors placeholder:text-muted-foreground focus-visible:border-foreground focus-visible:ring-2 focus-visible:ring-foreground/15 disabled:cursor-not-allowed disabled:opacity-50', props.class))
</script>

<template><input v-bind="attrs" :class="classes"></template>
