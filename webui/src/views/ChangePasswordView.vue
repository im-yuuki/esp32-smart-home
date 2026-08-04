<script setup lang="ts">
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { localizedError } from '@/i18n/errors'
import { useAuthStore } from '@/stores/auth'
import Button from '@/components/ui/Button.vue'
import Input from '@/components/ui/Input.vue'
import Label from '@/components/ui/Label.vue'
import Alert from '@/components/ui/Alert.vue'

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
  <main class="grid min-h-[calc(100vh-3.5rem)] place-items-center px-4 py-10">
    <form class="w-full max-w-md space-y-5 border border-[var(--app-border)] bg-[var(--app-surface)] p-5 sm:p-6" @submit.prevent="submit">
      <h1 class="text-xl font-semibold">{{ t('auth.password.title') }}</h1>
      <div class="space-y-1"><Label for="current-password">{{ t('auth.password.current') }}</Label><Input id="current-password" v-model="currentPassword" type="password" /></div>
      <div class="space-y-1"><Label for="new-password">{{ t('auth.password.new') }}</Label><Input id="new-password" v-model="newPassword" type="password" minlength="12" /><small class="text-xs text-[var(--app-text-muted)]">{{ t('auth.password.hint') }}</small></div>
      <div class="space-y-1"><Label for="confirm-password">{{ t('auth.password.confirm') }}</Label><Input id="confirm-password" v-model="confirmPassword" type="password" minlength="12" /></div>
      <Alert v-if="error" variant="destructive">{{ error }}</Alert>
      <Button type="submit" :loading="submitting" class="w-full">{{ t('auth.password.submit') }}</Button>
    </form>
  </main>
</template>
