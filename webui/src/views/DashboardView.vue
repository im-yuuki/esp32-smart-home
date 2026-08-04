<script setup lang="ts">
import { onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useFacilitiesStore } from '@/stores/facilities'
import AppIcon from '@/components/AppIcon.vue'

const facilities = useFacilitiesStore()
const router = useRouter()
onMounted(async () => {
  await facilities.load()
  const first = facilities.folders[0]
  if (first) await router.replace({ name: 'browse', params: { folderId: first.id } })
})
</script>

<template>
  <div class="grid min-h-[60vh] place-items-center"><AppIcon name="i-lucide-loader-circle" class="size-5 animate-spin" /><span class="sr-only">{{ $t('browse.loading') }}</span></div>
</template>
