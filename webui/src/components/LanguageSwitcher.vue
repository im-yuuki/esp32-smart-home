<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { setLocale } from '@/i18n'
import AppIcon from '@/components/AppIcon.vue'
import Button from '@/components/ui/Button.vue'
import { DropdownMenuContent, DropdownMenuItem, DropdownMenuPortal, DropdownMenuRoot, DropdownMenuTrigger } from 'reka-ui'

const { locale, t } = useI18n()
const currentLabel = computed(() => locale.value === 'vi' ? 'VI' : 'EN')

</script>

<template>
  <DropdownMenuRoot>
    <DropdownMenuTrigger as-child>
      <Button variant="ghost" size="icon" class="text-white hover:bg-white/15" :aria-label="t('language.current')" :title="t('language.current')"><AppIcon name="i-lucide-languages" class="size-4" /></Button>
    </DropdownMenuTrigger>
    <DropdownMenuPortal>
      <DropdownMenuContent side="right" align="start" :side-offset="8" class="z-[100] min-w-36 rounded-md border border-border bg-card p-1 text-card-foreground shadow-xl">
        <DropdownMenuItem class="flex cursor-pointer items-center justify-between rounded px-2 py-1.5 text-xs outline-none hover:bg-muted" @select="setLocale('en')">English <span v-if="locale === 'en'" class="font-mono">{{ currentLabel }}</span></DropdownMenuItem>
        <DropdownMenuItem class="flex cursor-pointer items-center justify-between rounded px-2 py-1.5 text-xs outline-none hover:bg-muted" @select="setLocale('vi')">Tiếng Việt <span v-if="locale === 'vi'" class="font-mono">{{ currentLabel }}</span></DropdownMenuItem>
      </DropdownMenuContent>
    </DropdownMenuPortal>
  </DropdownMenuRoot>
</template>
