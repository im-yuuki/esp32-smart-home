<script setup lang="ts">
import { CheckboxIndicator, CheckboxRoot } from 'reka-ui'
import AppIcon from '@/components/AppIcon.vue'

const props = defineProps<{ modelValue?: boolean | unknown[]; checked?: boolean; value?: unknown; disabled?: boolean }>()
const emit = defineEmits<{ 'update:modelValue': [value: boolean | unknown[]]; 'update:checked': [value: boolean] }>()
const isChecked = () => Array.isArray(props.modelValue) ? props.modelValue.includes(props.value) : Boolean(props.checked ?? props.modelValue)
function update(value: boolean) {
  if (Array.isArray(props.modelValue)) {
    const next = props.modelValue.filter((item) => item !== props.value)
    if (value) next.push(props.value)
    emit('update:modelValue', next)
  } else emit('update:modelValue', value)
  emit('update:checked', value)
}
</script>
<template>
  <CheckboxRoot :checked="isChecked()" :disabled="disabled" class="grid size-4 shrink-0 place-items-center rounded-sm border border-border bg-background text-background focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-foreground/30 data-[state=checked]:border-foreground data-[state=checked]:bg-foreground disabled:opacity-50" @update:checked="update">
    <CheckboxIndicator><AppIcon name="i-lucide-check" class="size-3" /></CheckboxIndicator>
  </CheckboxRoot>
</template>
