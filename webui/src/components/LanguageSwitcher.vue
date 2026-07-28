<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { setLocale, type AppLocale } from '@/i18n'

const { locale, t } = useI18n()
const currentLabel = computed(() => locale.value === 'vi' ? 'VI' : 'EN')

function toggleLocale(): void {
  setLocale((locale.value === 'vi' ? 'en' : 'vi') as AppLocale)
}
</script>

<template>
  <UButton
    color="neutral"
    variant="ghost"
    icon="i-lucide-languages"
    :aria-label="t('language.switchTo')"
    :title="t('language.current')"
    @click="toggleLocale"
  >
    {{ currentLabel }}
  </UButton>
</template>
