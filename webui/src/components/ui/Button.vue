<script setup lang="ts">
import { computed } from 'vue'
import { cn } from '@/lib/utils'

type Variant = 'default' | 'secondary' | 'outline' | 'ghost' | 'link' | 'destructive'
type Size = 'default' | 'sm' | 'lg' | 'icon'

const props = withDefaults(defineProps<{
  variant?: Variant
  size?: Size
  loading?: boolean
  disabled?: boolean
  class?: string
  type?: 'button' | 'submit' | 'reset'
}>(), { variant: 'default', size: 'default', type: 'button' })

const emit = defineEmits<{ click: [event: MouseEvent] }>()
const classes = computed(() => cn(
  'inline-flex items-center justify-center gap-2 whitespace-nowrap rounded-md text-sm font-medium transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-foreground/30 disabled:pointer-events-none disabled:opacity-50',
  props.variant === 'default' && 'bg-foreground text-background hover:bg-foreground/90',
  props.variant === 'secondary' && 'bg-muted text-foreground hover:bg-muted/70',
  props.variant === 'outline' && 'border border-border bg-background hover:bg-muted',
  props.variant === 'ghost' && 'hover:bg-muted',
  props.variant === 'link' && 'text-foreground underline-offset-4 hover:underline',
  props.variant === 'destructive' && 'bg-danger text-white hover:bg-danger/90',
  props.size === 'default' && 'h-9 px-4 py-2', props.size === 'sm' && 'h-8 rounded-md px-3 text-xs',
  props.size === 'lg' && 'h-10 px-6', props.size === 'icon' && 'size-9', props.class,
))
</script>

<template>
  <button :type="type" :class="classes" :disabled="disabled || loading" @click="emit('click', $event)">
    <span v-if="loading" class="size-3 animate-spin rounded-full border-2 border-current border-r-transparent" aria-hidden="true" />
    <slot />
  </button>
</template>
