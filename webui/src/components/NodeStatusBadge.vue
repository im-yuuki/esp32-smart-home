<script setup lang="ts">
defineProps<{ online: boolean }>()
</script>

<template>
  <UBadge :color="online ? 'success' : 'neutral'" variant="subtle" size="sm">
    {{ online ? 'Online' : 'Offline' }}
  </UBadge>
</template>
