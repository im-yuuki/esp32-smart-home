<script setup lang="ts">
import { ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const route = useRoute()
const router = useRouter()
const auth = useAuthStore()
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
    error.value = e instanceof Error ? e.message : String(e)
  } finally {
    submitting.value = false
  }
}
</script>

<template>
  <main class="grid min-h-[calc(100vh-3.5rem)] place-items-center px-4 py-12">
    <UCard class="w-full max-w-sm">
      <template #header>
        <div class="space-y-1">
          <p class="text-xs font-medium uppercase tracking-[0.18em] text-primary">Server access</p>
          <h1 class="text-xl font-semibold text-highlighted">Sign in to Smart Home</h1>
          <p class="text-sm text-muted">Accounts and permissions stay on this management server.</p>
        </div>
      </template>
      <form class="space-y-4" @submit.prevent="submit">
        <UFormField label="Username">
          <UInput v-model="username" autocomplete="username" autofocus class="w-full" />
        </UFormField>
        <UFormField label="Password">
          <UInput v-model="password" type="password" autocomplete="current-password" class="w-full" />
        </UFormField>
        <UAlert v-if="error" color="error" variant="subtle" :description="error" />
        <UButton type="submit" block :loading="submitting">Sign in</UButton>
      </form>
    </UCard>
  </main>
</template>
