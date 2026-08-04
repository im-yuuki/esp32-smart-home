<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import type { RelayState } from '@/types/api'
import Switch from '@/components/ui/Switch.vue'

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
const { t } = useI18n()
</script>

<template>
  <!-- min-h-11 keeps the row at a >=44px touch target -->
  <div class="flex min-h-11 items-center justify-between gap-3 py-1">
    <span class="text-sm">{{ label }}</span>
    <div class="flex items-center gap-2">
      <span v-if="pending" class="size-3 animate-spin rounded-full border-2 border-foreground border-r-transparent" aria-hidden="true" />
      <Switch :model-value="state === 'ON'" active :disabled="disabled || pending" :aria-label="t('relay.toggle', { label })" @update:model-value="emit('toggle')" />
    </div>
  </div>
</template>
