<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import * as adminApi from '@/api/admin'
import { localizedError } from '@/i18n/errors'
import type { AdminGroup, AdminNode, AdminUser } from '@/types/admin'

type Tab = 'nodes' | 'users' | 'groups'
const tab = ref<Tab>('nodes')
const loading = ref(true)
const error = ref<string | null>(null)
const users = ref<AdminUser[]>([])
const groups = ref<AdminGroup[]>([])
const pendingNodes = ref<AdminNode[]>([])
const approvedNodes = ref<AdminNode[]>([])
const permissions = ref<string[]>([])
const pendingDrafts = ref<Record<string, number[]>>({})
const nodeGroupDrafts = ref<Record<string, number[]>>({})
const { locale, t, te } = useI18n()

const newUsername = ref('')
const newDisplayName = ref('')
const temporaryPassword = ref('')
const newSystemAdmin = ref(false)
const newGroupName = ref('')
const newGroupDescription = ref('')
const selectedGroupId = ref<number | null>(null)
const newRoleName = ref('')
const newRolePermissions = ref<string[]>([])
const assignUserId = ref<number | null>(null)
const assignRoleId = ref<number | null>(null)

const selectedGroup = computed(() => groups.value.find((group) => group.id === selectedGroupId.value) ?? null)
const selectedGroupNodeList = computed(() => {
  const ids = selectedGroup.value?.nodeIds ?? []
  return ids.length ? new Intl.ListFormat(locale.value).format(ids) : t('common.none')
})

function permissionLabel(permission: string): string {
  const key = `admin.permissions.${permission}`
  return te(key) ? t(key) : permission
}

function permissionList(values: string[]): string {
  return new Intl.ListFormat(locale.value).format(values.map(permissionLabel))
}

async function load(): Promise<void> {
  loading.value = true
  error.value = null
  try {
    const [nextUsers, nextGroups, nextPending, nextApproved, nextPermissions] = await Promise.all([
      adminApi.listUsers(), adminApi.listGroups(), adminApi.listAdminNodes('PENDING'),
      adminApi.listAdminNodes('APPROVED'), adminApi.listPermissions(),
    ])
    users.value = nextUsers
    groups.value = nextGroups
    pendingNodes.value = nextPending
    approvedNodes.value = nextApproved
    permissions.value = nextPermissions
    pendingDrafts.value = Object.fromEntries(nextPending.map((node) => [node.nodeId, [...node.groupIds]]))
    nodeGroupDrafts.value = Object.fromEntries(nextApproved.map((node) => [node.nodeId, [...node.groupIds]]))
    if (selectedGroupId.value === null && nextGroups[0]) selectedGroupId.value = nextGroups[0].id
  } catch (e) {
    error.value = localizedError(e)
  } finally {
    loading.value = false
  }
}

async function run(action: () => Promise<void>): Promise<boolean> {
  error.value = null
  try {
    await action()
    await load()
    return true
  } catch (e) {
    error.value = localizedError(e)
    return false
  }
}

async function submitUser(): Promise<void> {
  const succeeded = await run(() => adminApi.createUser({ username: newUsername.value, displayName: newDisplayName.value,
    temporaryPassword: temporaryPassword.value, systemAdmin: newSystemAdmin.value }))
  if (!succeeded) return
  newUsername.value = ''; newDisplayName.value = ''; temporaryPassword.value = ''; newSystemAdmin.value = false
}

async function submitGroup(): Promise<void> {
  const succeeded = await run(() => adminApi.createGroup({ name: newGroupName.value, description: newGroupDescription.value }))
  if (!succeeded) return
  newGroupName.value = ''; newGroupDescription.value = ''
}

async function submitRole(): Promise<void> {
  if (selectedGroupId.value === null) return
  const succeeded = await run(async () => { await adminApi.createRole(selectedGroupId.value!, { name: newRoleName.value, permissions: newRolePermissions.value }) })
  if (!succeeded) return
  newRoleName.value = ''; newRolePermissions.value = []
}

async function submitAssignment(): Promise<void> {
  if (selectedGroupId.value === null || assignUserId.value === null || assignRoleId.value === null) return
  await run(() => adminApi.assignRole(selectedGroupId.value!, assignUserId.value!, assignRoleId.value!))
}

onMounted(load)
watch(selectedGroupId, () => {
  assignRoleId.value = null
})
</script>

<template>
  <UContainer class="space-y-6 py-6">
    <div class="flex flex-wrap items-end justify-between gap-4">
      <div>
        <p class="text-xs font-medium uppercase tracking-[0.18em] text-primary">{{ t('admin.eyebrow') }}</p>
        <h1 class="text-2xl font-semibold text-highlighted">{{ t('admin.title') }}</h1>
      </div>
      <UButton to="/" color="neutral" variant="soft" icon="i-lucide-arrow-left">{{ t('common.dashboard') }}</UButton>
    </div>
    <div class="flex gap-2 border-b border-default pb-3">
      <UButton
        v-for="item in (['nodes', 'users', 'groups'] as Tab[])"
        :key="item"
        :variant="tab === item ? 'solid' : 'ghost'"
        color="neutral"
        @click="tab = item"
      >
        {{ t(`admin.tabs.${item}`) }}
      </UButton>
    </div>
    <UAlert v-if="error" color="error" variant="subtle" :title="t('admin.requestFailed')" :description="error" />
    <USkeleton v-if="loading" class="h-72 w-full rounded-lg" />

    <div v-else-if="tab === 'nodes'" class="grid gap-6 lg:grid-cols-2">
      <UCard>
        <template #header><h2 class="font-semibold text-highlighted">{{ t('admin.nodes.pending') }}</h2></template>
        <p v-if="pendingNodes.length === 0" class="text-sm text-muted">{{ t('admin.nodes.noPending') }}</p>
        <div v-for="node in pendingNodes" :key="node.nodeId" class="space-y-3 border-b border-default py-4 last:border-0">
          <div>
            <p class="font-medium">{{ node.nodeId }}</p>
            <p class="text-xs text-muted">{{ node.room }} · {{ node.ip ?? t('common.noIp') }}</p>
          </div>
          <div class="flex flex-wrap gap-3">
            <label v-for="group in groups" :key="group.id" class="flex items-center gap-2 text-sm">
              <input v-model="pendingDrafts[node.nodeId]" type="checkbox" :value="group.id">{{ group.name }}
            </label>
          </div>
          <div class="flex gap-2">
            <UButton size="sm" :disabled="!pendingDrafts[node.nodeId]?.length" @click="run(() => adminApi.approveNode(node.nodeId, pendingDrafts[node.nodeId] ?? []))">{{ t('admin.nodes.approve') }}</UButton>
            <UButton size="sm" color="error" variant="soft" @click="run(() => adminApi.rejectNode(node.nodeId))">{{ t('admin.nodes.reject') }}</UButton>
          </div>
        </div>
      </UCard>
      <UCard>
        <template #header><h2 class="font-semibold text-highlighted">{{ t('admin.nodes.approvedGroups') }}</h2></template>
        <p v-if="approvedNodes.length === 0" class="text-sm text-muted">{{ t('admin.nodes.noApproved') }}</p>
        <div v-for="node in approvedNodes" :key="node.nodeId" class="space-y-3 border-b border-default py-4 last:border-0">
          <p class="font-medium">{{ node.nodeId }}</p>
          <div class="flex flex-wrap gap-3">
            <label v-for="group in groups" :key="group.id" class="flex items-center gap-2 text-sm">
              <input v-model="nodeGroupDrafts[node.nodeId]" type="checkbox" :value="group.id">{{ group.name }}
            </label>
          </div>
          <UButton size="sm" variant="soft" :disabled="!nodeGroupDrafts[node.nodeId]?.length" @click="run(() => adminApi.setNodeGroups(node.nodeId, nodeGroupDrafts[node.nodeId] ?? []))">{{ t('admin.nodes.saveGroups') }}</UButton>
        </div>
      </UCard>
    </div>

    <div v-else-if="tab === 'users'" class="grid gap-6 lg:grid-cols-[minmax(0,1fr)_22rem]">
      <UCard>
        <template #header><h2 class="font-semibold text-highlighted">{{ t('admin.users.accounts') }}</h2></template>
        <div v-for="user in users" :key="user.id" class="flex items-center justify-between gap-4 border-b border-default py-3 last:border-0">
          <div>
            <p class="font-medium">{{ user.displayName }} <span v-if="user.systemAdmin" class="text-xs text-primary">{{ t('admin.users.systemAdmin') }}</span></p>
            <p class="text-xs text-muted">{{ user.username }} · {{ user.mustChangePassword ? t('admin.users.passwordChangeRequired') : t('admin.users.activeCredentials') }}</p>
          </div>
          <UButton size="sm" :color="user.enabled ? 'error' : 'primary'" variant="soft" @click="run(() => adminApi.setUserEnabled(user.id, !user.enabled))">{{ user.enabled ? t('admin.users.disable') : t('admin.users.enable') }}</UButton>
        </div>
      </UCard>
      <UCard>
        <template #header><h2 class="font-semibold text-highlighted">{{ t('admin.users.createTitle') }}</h2></template>
        <form class="space-y-3" @submit.prevent="submitUser">
          <UInput v-model="newUsername" :placeholder="t('admin.users.username')" class="w-full" required />
          <UInput v-model="newDisplayName" :placeholder="t('admin.users.displayName')" class="w-full" required />
          <UInput v-model="temporaryPassword" type="password" minlength="12" :placeholder="t('admin.users.temporaryPassword')" class="w-full" required />
          <label class="flex items-center gap-2 text-sm"><input v-model="newSystemAdmin" type="checkbox">{{ t('admin.users.systemAdministrator') }}</label>
          <UButton type="submit" block>{{ t('admin.users.create') }}</UButton>
        </form>
      </UCard>
    </div>

    <div v-else class="grid gap-6 lg:grid-cols-[18rem_minmax(0,1fr)]">
      <div class="space-y-4">
        <UCard>
          <template #header><h2 class="font-semibold text-highlighted">{{ t('admin.groups.createTitle') }}</h2></template>
          <form class="space-y-3" @submit.prevent="submitGroup">
            <UInput v-model="newGroupName" :placeholder="t('admin.groups.name')" class="w-full" required />
            <UTextarea v-model="newGroupDescription" :placeholder="t('admin.groups.description')" class="w-full" />
            <UButton type="submit" block>{{ t('admin.groups.create') }}</UButton>
          </form>
        </UCard>
        <select v-model="selectedGroupId" class="w-full rounded-md border border-default bg-default px-3 py-2 text-sm">
          <option v-for="group in groups" :key="group.id" :value="group.id">{{ group.name }}</option>
        </select>
      </div>
      <div v-if="selectedGroup" class="grid gap-6 xl:grid-cols-2">
        <UCard>
          <template #header><h2 class="font-semibold text-highlighted">{{ t('admin.groups.rolesIn', { group: selectedGroup.name }) }}</h2></template>
          <div v-for="role in selectedGroup.roles" :key="role.id" class="border-b border-default py-3 last:border-0">
            <p class="font-medium">{{ role.name }}</p>
            <p class="text-xs text-muted">{{ permissionList(role.permissions) }}</p>
          </div>
          <form class="mt-4 space-y-3 border-t border-default pt-4" @submit.prevent="submitRole">
            <UInput v-model="newRoleName" :placeholder="t('admin.groups.newRoleName')" class="w-full" required />
            <label v-for="permission in permissions" :key="permission" class="flex items-center gap-2 text-sm">
              <input v-model="newRolePermissions" type="checkbox" :value="permission">{{ permissionLabel(permission) }}
            </label>
            <UButton type="submit" size="sm">{{ t('admin.groups.createRole') }}</UButton>
          </form>
        </UCard>
        <UCard>
          <template #header><h2 class="font-semibold text-highlighted">{{ t('admin.groups.members') }}</h2></template>
          <div v-for="member in selectedGroup.members" :key="member.userId" class="flex items-center justify-between gap-3 border-b border-default py-3 last:border-0">
            <div><p class="font-medium">{{ member.displayName }}</p><p class="text-xs text-muted">{{ member.roleName }}</p></div>
            <UButton size="xs" color="error" variant="ghost" @click="run(() => adminApi.removeMember(selectedGroup!.id, member.userId))">{{ t('admin.groups.remove') }}</UButton>
          </div>
          <form class="mt-4 grid gap-3 border-t border-default pt-4 sm:grid-cols-2" @submit.prevent="submitAssignment">
            <select v-model="assignUserId" required class="rounded-md border border-default bg-default px-3 py-2 text-sm">
              <option :value="null" disabled>{{ t('admin.groups.user') }}</option>
              <option v-for="user in users" :key="user.id" :value="user.id">{{ user.displayName }}</option>
            </select>
            <select v-model="assignRoleId" required class="rounded-md border border-default bg-default px-3 py-2 text-sm">
              <option :value="null" disabled>{{ t('admin.groups.role') }}</option>
              <option v-for="role in selectedGroup.roles" :key="role.id" :value="role.id">{{ role.name }}</option>
            </select>
            <UButton type="submit" class="sm:col-span-2">{{ t('admin.groups.assignRole') }}</UButton>
          </form>
          <p class="mt-5 text-xs text-muted">{{ t('admin.groups.nodes', { nodes: selectedGroupNodeList }) }}</p>
        </UCard>
      </div>
    </div>
  </UContainer>
</template>
