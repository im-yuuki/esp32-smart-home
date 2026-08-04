<script setup lang="ts">
import { ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { localizedError } from '@/i18n/errors'
import { useAuthStore } from '@/stores/auth'
import Button from '@/components/ui/Button.vue'
import Input from '@/components/ui/Input.vue'
import Label from '@/components/ui/Label.vue'
import Alert from '@/components/ui/Alert.vue'

const route = useRoute()
const router = useRouter()
const auth = useAuthStore()
const { t } = useI18n()
const username = ref('')
const password = ref('')
const submitting = ref(false)
const error = ref<string | null>(null)

async function submit(): Promise<void> {
  if (!username.value.trim() || !password.value) return
  submitting.value = true
  error.value = null
  try {
    await auth.login(username.value, password.value)
    if (auth.user?.mustChangePassword) {
      await router.replace({ name: 'change-password' })
      return
    }
    const requested = typeof route.query.redirect === 'string' ? route.query.redirect : '/'
    const destination = requested.startsWith('/') && !requested.startsWith('//') ? requested : '/'
    await router.replace(destination)
  } catch (e) {
    error.value = localizedError(e)
  } finally {
    submitting.value = false
  }
}
</script>

<template>
  <main class="grid min-h-[calc(100vh-3.5rem)] place-items-center px-4 py-10">
    <form class="w-full max-w-sm space-y-5 border border-[var(--app-border)] bg-[var(--app-surface)] p-5 sm:p-6" @submit.prevent="submit">
      <div><p class="section-kicker">{{ t('auth.login.eyebrow') }}</p><h1 class="text-xl font-semibold">{{ t('auth.login.title') }}</h1></div>
      <div class="space-y-1"><Label for="username">{{ t('auth.login.username') }}</Label><Input id="username" v-model="username" autocomplete="username" autofocus /></div>
      <div class="space-y-1"><Label for="password">{{ t('auth.login.password') }}</Label><Input id="password" v-model="password" type="password" autocomplete="current-password" /></div>
      <Alert v-if="error" variant="destructive">{{ error }}</Alert>
      <Button type="submit" :loading="submitting" class="w-full">{{ t('auth.login.submit') }}</Button>
    </form>
  </main>
</template>
