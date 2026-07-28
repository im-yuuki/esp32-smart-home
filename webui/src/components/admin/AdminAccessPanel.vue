<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import * as api from '@/api/admin'
import { localizedError } from '@/i18n/errors'
import type { AdminFolder, AdminUser } from '@/types/admin'

const { t } = useI18n()
const folders = ref<AdminFolder[]>([])
const users = ref<AdminUser[]>([])
const permissions = ref<string[]>([])
const folderId = ref<string | null>(null)
const roleName = ref('')
const rolePermissions = ref<string[]>([])
const userId = ref<number | null>(null)
const roleId = ref<string | null>(null)
const busy = ref(false)
const error = ref<string | null>(null)
const selectedFolder = computed(() => folders.value.find((folder) => folder.id === folderId.value) ?? null)

async function load() {
  busy.value = true
  error.value = null
  try {
    ;[folders.value, users.value, permissions.value] = await Promise.all([api.listAdminFolders(), api.listUsers(), api.listPermissions()])
    if (!folderId.value || !folders.value.some((folder) => folder.id === folderId.value)) folderId.value = folders.value[0]?.id ?? null
  } catch (cause) { error.value = localizedError(cause) } finally { busy.value = false }
}
async function run(action: () => Promise<void>) { busy.value = true; error.value = null; try { await action(); await load() } catch (cause) { error.value = localizedError(cause); busy.value = false } }
async function createRole() { if (!folderId.value) return; await run(() => api.createFolderRole(folderId.value!, { name: roleName.value, permissions: rolePermissions.value })); roleName.value = ''; rolePermissions.value = [] }
async function assign() { if (!folderId.value || userId.value == null || !roleId.value) return; await run(() => api.assignFolderRole(folderId.value!, userId.value!, roleId.value!)) }
onMounted(load)
</script>

<template>
  <div class="space-y-4">
    <UAlert v-if="error" color="error" variant="subtle" :description="error" />
    <div class="grid gap-6 xl:grid-cols-[18rem_1fr_1fr]">
      <aside><label class="field-label">{{ t('admin.access.folder') }}</label><select v-model="folderId" class="form-control w-full"><option v-for="folder in folders" :key="folder.id" :value="folder.id">{{ folder.name }}</option></select><p v-if="selectedFolder" class="mt-3 text-xs text-muted">{{ t('admin.access.nodes', { count: selectedFolder.nodeIds.length }) }}</p></aside>
      <section><h2 class="mb-3 text-lg font-semibold">{{ t('admin.access.roles') }}</h2><USkeleton v-if="busy && !folders.length" class="h-40 w-full" /><template v-else><div v-for="role in selectedFolder?.roles ?? []" :key="role.id" class="border-b border-default py-3"><strong>{{ role.name }}</strong><p class="text-xs text-muted">{{ role.permissions.join(' · ') }}</p></div><form class="mt-4 space-y-3 border-t border-default pt-4" @submit.prevent="createRole"><UInput v-model="roleName" :placeholder="t('admin.access.roleName')" class="w-full" required /><label v-for="permission in permissions" :key="permission" class="check-row"><input v-model="rolePermissions" type="checkbox" :value="permission">{{ permission }}</label><UButton type="submit" :loading="busy" :disabled="!rolePermissions.length">{{ t('admin.access.createRole') }}</UButton></form></template></section>
      <section><h2 class="mb-3 text-lg font-semibold">{{ t('admin.access.memberships') }}</h2><div v-for="member in selectedFolder?.members ?? []" :key="member.userId" class="flex items-center justify-between border-b border-default py-3"><div><strong class="block">{{ member.displayName }}</strong><span class="text-xs text-muted">{{ member.roleName }}</span></div><UButton color="error" variant="ghost" size="xs" @click="folderId && run(() => api.removeFolderMember(folderId!, member.userId))">{{ t('common.remove') }}</UButton></div><form class="mt-4 grid gap-3 border-t border-default pt-4" @submit.prevent="assign"><select v-model="userId" class="form-control" required><option :value="null" disabled>{{ t('admin.access.user') }}</option><option v-for="user in users" :key="user.id" :value="user.id">{{ user.displayName }}</option></select><select v-model="roleId" class="form-control" required><option :value="null" disabled>{{ t('admin.access.role') }}</option><option v-for="role in selectedFolder?.roles ?? []" :key="role.id" :value="role.id">{{ role.name }}</option></select><UButton type="submit" :loading="busy">{{ t('admin.access.assign') }}</UButton></form></section>
    </div>
  </div>
</template>
