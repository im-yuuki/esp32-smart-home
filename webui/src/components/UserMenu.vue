<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRouter } from 'vue-router'
import { DropdownMenuContent, DropdownMenuItem, DropdownMenuLabel, DropdownMenuPortal, DropdownMenuRoot, DropdownMenuSeparator, DropdownMenuTrigger } from 'reka-ui'
import { useAuthStore } from '@/stores/auth'
import Button from '@/components/ui/Button.vue'
import AppIcon from '@/components/AppIcon.vue'

const auth = useAuthStore()
const router = useRouter()
const { t } = useI18n()
const initials = computed(() => (auth.user?.displayName || auth.user?.username || '?').slice(0, 1).toUpperCase())

async function logout(): Promise<void> {
  try { await auth.logout() } finally { await router.replace({ name: 'login' }) }
}
</script>

<template>
  <DropdownMenuRoot>
    <DropdownMenuTrigger as-child>
      <Button variant="ghost" size="icon" class="text-white hover:bg-white/15" :aria-label="t('account.menu')" :title="auth.user?.displayName || auth.user?.username">
        <span class="grid size-6 place-items-center rounded-full border border-white/60 text-[10px] font-semibold">{{ initials }}</span>
      </Button>
    </DropdownMenuTrigger>
    <DropdownMenuPortal>
      <DropdownMenuContent side="right" align="end" :side-offset="8" class="z-[100] w-60 rounded-md border border-border bg-card p-1 text-card-foreground shadow-xl">
        <DropdownMenuLabel class="px-2 py-2"><p class="truncate text-sm font-semibold">{{ auth.user?.displayName }}</p><p class="truncate font-mono text-[10px] text-muted-foreground">@{{ auth.user?.username }}</p><p class="mt-2 text-[10px] uppercase tracking-wider text-muted-foreground">{{ auth.user?.systemAdmin ? t('account.systemAdmin') : auth.user?.canViewAudit ? t('account.auditAccess') : t('account.standardAccess') }}</p></DropdownMenuLabel>
        <DropdownMenuSeparator class="my-1 h-px bg-border" />
        <DropdownMenuItem class="flex cursor-pointer items-center gap-2 rounded px-2 py-2 text-xs outline-none hover:bg-muted" @select="router.push({ name: 'change-password' })"><AppIcon name="i-lucide-key-round" class="size-4" />{{ t('account.changePassword') }}</DropdownMenuItem>
        <DropdownMenuItem class="flex cursor-pointer items-center gap-2 rounded px-2 py-2 text-xs text-danger outline-none hover:bg-danger/10" @select="logout"><AppIcon name="i-lucide-log-out" class="size-4" />{{ t('account.signOut') }}</DropdownMenuItem>
      </DropdownMenuContent>
    </DropdownMenuPortal>
  </DropdownMenuRoot>
</template>
