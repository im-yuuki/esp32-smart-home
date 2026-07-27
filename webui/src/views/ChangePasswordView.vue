<script setup lang="ts">
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const auth = useAuthStore()
const router = useRouter()
const currentPassword = ref('')
const newPassword = ref('')
const confirmPassword = ref('')
const submitting = ref(false)
const error = ref<string | null>(null)

async function submit(): Promise<void> {
  if (newPassword.value !== confirmPassword.value) {
    error.value = 'New passwords do not match.'
    return
  }
  submitting.value = true
  error.value = null
  try {
    await auth.changePassword(currentPassword.value, newPassword.value)
    await router.replace({ name: 'login' })
  } catch (e) {
    error.value = e instanceof Error ? e.message : String(e)
  } finally {
    submitting.value = false
  }
}
</script>

<template>
  <UContainer class="grid min-h-[calc(100vh-3.5rem)] place-items-center py-12">
    <UCard class="w-full max-w-md">
      <template #header>
        <h1 class="text-xl font-semibold text-highlighted">Set a new password</h1>
        <p class="mt-1 text-sm text-muted">Your temporary password must be replaced before accessing nodes.</p>
      </template>
      <form class="space-y-4" @submit.prevent="submit">
        <UFormField label="Current password"><UInput v-model="currentPassword" type="password" class="w-full" /></UFormField>
        <UFormField label="New password" hint="At least 12 characters"><UInput v-model="newPassword" type="password" minlength="12" class="w-full" /></UFormField>
        <UFormField label="Confirm new password"><UInput v-model="confirmPassword" type="password" minlength="12" class="w-full" /></UFormField>
        <UAlert v-if="error" color="error" variant="subtle" :description="error" />
        <UButton type="submit" block :loading="submitting">Change password</UButton>
      </form>
    </UCard>
  </UContainer>
</template>
