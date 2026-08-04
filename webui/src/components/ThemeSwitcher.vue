<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import AppIcon from '@/components/AppIcon.vue'
import { useTheme } from '@/composables/useTheme'
import Button from '@/components/ui/Button.vue'

const { t } = useI18n()
const { mode, toggle } = useTheme()
</script>

<template>
  <Button variant="ghost" size="icon" class="text-white hover:bg-white/15" :aria-label="t('theme.switch')" :title="t('theme.switch')" @click="toggle">
    <AppIcon :name="mode === 'dark' ? 'i-lucide-sun' : 'i-lucide-moon'" class="size-4" />
  </Button>
</template>
