<script setup lang="ts">
import ConnectionIndicator from '@/components/ConnectionIndicator.vue'
</script>

<template>
  <header class="sticky top-0 z-40 border-b border-default bg-default/80 backdrop-blur">
    <UContainer class="flex h-14 items-center justify-between">
      <RouterLink to="/" class="flex items-center gap-2 font-semibold text-highlighted">
        <UIcon name="i-lucide-house" class="size-5 text-primary" />
        <span>Smart Home</span>
      </RouterLink>
      <ConnectionIndicator />
    </UContainer>
  </header>
</template>
