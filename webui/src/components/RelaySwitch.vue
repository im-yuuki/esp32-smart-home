<script setup lang="ts">
import type { RelayState } from '@/types/api'

/**
 * Dumb component: props in, `toggle` out. The pending/timeout machine lives in
 * useNodesStore. Never flips optimistically — the visual state only changes
 * when the store applies a RELAY_STATE event (or reverts on timeout).
 */
defineProps<{
  label: string
  state: RelayState
  pending: boolean
  disabled: boolean
}>()

const emit = defineEmits<{ toggle: [] }>()
</script>

<template>
  <!-- min-h-11 keeps the row at a >=44px touch target -->
  <div class="flex min-h-11 items-center justify-between gap-3 py-1">
    <span class="text-sm text-default">{{ label }}</span>
    <USwitch
      :model-value="state === 'ON'"
      :loading="pending"
      :disabled="disabled || pending"
      :aria-label="`Toggle ${label}`"
      @update:model-value="emit('toggle')"
    />
  </div>
</template>
