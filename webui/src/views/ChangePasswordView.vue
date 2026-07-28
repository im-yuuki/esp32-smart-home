<script setup lang="ts">
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { localizedError } from '@/i18n/errors'
import { useAuthStore } from '@/stores/auth'

const auth = useAuthStore()
const router = useRouter()
const { t } = useI18n()
const currentPassword = ref('')
const newPassword = ref('')
const confirmPassword = ref('')
const submitting = ref(false)
const error = ref<string | null>(null)

async function submit(): Promise<void> {
  if (newPassword.value !== confirmPassword.value) {
    error.value = t('auth.password.mismatch')
    return
  }
  submitting.value = true
  error.value = null
  try {
    await auth.changePassword(currentPassword.value, newPassword.value)
    await router.replace({ name: 'login' })
  } catch (e) {
    error.value = localizedError(e)
  } finally {
    submitting.value = false
  }
}
</script>

<template>
  <UContainer class="grid min-h-[calc(100vh-3.5rem)] place-items-center py-12">
    <UCard class="w-full max-w-md">
      <template #header>
        <h1 class="text-xl font-semibold text-highlighted">{{ t('auth.password.title') }}</h1>
        <p class="mt-1 text-sm text-muted">{{ t('auth.password.description') }}</p>
      </template>
      <form class="space-y-4" @submit.prevent="submit">
        <UFormField :label="t('auth.password.current')"><UInput v-model="currentPassword" type="password" class="w-full" /></UFormField>
        <UFormField :label="t('auth.password.new')" :hint="t('auth.password.hint')"><UInput v-model="newPassword" type="password" minlength="12" class="w-full" /></UFormField>
        <UFormField :label="t('auth.password.confirm')"><UInput v-model="confirmPassword" type="password" minlength="12" class="w-full" /></UFormField>
        <UAlert v-if="error" color="error" variant="subtle" :description="error" />
        <UButton type="submit" block :loading="submitting">{{ t('auth.password.submit') }}</UButton>
      </form>
    </UCard>
  </UContainer>
</template>
