<script setup lang="ts">
import { useI18n } from 'vue-i18n'
import Badge from '@/components/ui/Badge.vue'

defineProps<{ online: boolean }>()
const { t } = useI18n()
</script>

<template>
  <Badge :variant="online ? 'success' : 'secondary'">{{ online ? t('node.online') : t('node.offline') }}</Badge>
</template>
