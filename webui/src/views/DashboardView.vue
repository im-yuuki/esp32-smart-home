<script setup lang="ts">
import { onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useFacilitiesStore } from '@/stores/facilities'

const facilities = useFacilitiesStore()
const router = useRouter()
onMounted(async () => {
  await facilities.load()
  const first = facilities.folders[0]
  if (first) await router.replace({ name: 'browse', params: { folderId: first.id } })
})
</script>

<template>
  <div class="grid min-h-[60vh] place-items-center"><div class="space-y-3 text-center"><UIcon name="i-lucide-loader-circle" class="mx-auto size-6 animate-spin text-primary" /><p class="text-sm text-muted">{{ $t('browse.loading') }}</p></div></div>
</template>
