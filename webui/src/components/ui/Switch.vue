<script setup lang="ts">
import { SwitchRoot, SwitchThumb } from 'reka-ui'
import { cn } from '@/lib/utils'

const props = defineProps<{ modelValue: boolean; disabled?: boolean; active?: boolean }>()
const emit = defineEmits<{ 'update:modelValue': [value: boolean] }>()
</script>
<template>
  <SwitchRoot :checked="modelValue" :disabled="disabled" class="peer inline-flex h-5 w-9 shrink-0 cursor-pointer items-center rounded-full border-2 border-transparent bg-muted transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-foreground/30 disabled:cursor-not-allowed disabled:opacity-50" :class="active ? 'data-[state=checked]:bg-success' : 'data-[state=checked]:bg-foreground'" @update:checked="emit('update:modelValue', $event)">
    <SwitchThumb :class="cn('pointer-events-none block size-4 rounded-full bg-background shadow-sm ring-0 transition-transform data-[state=checked]:translate-x-4 data-[state=unchecked]:translate-x-0')" />
  </SwitchRoot>
</template>
