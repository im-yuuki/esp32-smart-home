<script setup lang="ts">
import { useI18n } from 'vue-i18n'

defineProps<{ online: boolean }>()
const { t } = useI18n()
</script>

<template>
  <UBadge :color="online ? 'success' : 'neutral'" variant="subtle" size="sm">
    {{ online ? t('node.online') : t('node.offline') }}
  </UBadge>
</template>
