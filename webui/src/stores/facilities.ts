import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import * as api from '@/api/facilities'
import { localizedError } from '@/i18n/errors'
import type { DeviceType, Tag } from '@/types/api'
import type { FolderDto } from '@/types/facility'

export interface FolderTreeNode extends FolderDto { children: FolderTreeNode[] }

export const useFacilitiesStore = defineStore('facilities', () => {
  const folders = ref<FolderDto[]>([])
  const deviceTypes = ref<DeviceType[]>([])
  const tags = ref<Tag[]>([])
  const loading = ref(false)
  const loaded = ref(false)
  const error = ref<string | null>(null)

  const folderById = computed(() => new Map(folders.value.map((folder) => [folder.id, folder])))
  const tree = computed<FolderTreeNode[]>(() => {
    const nodes = new Map(folders.value.map((folder) => [folder.id, { ...folder, children: [] } as FolderTreeNode]))
    const roots: FolderTreeNode[] = []
    for (const node of nodes.values()) {
      const parent = node.parentId ? nodes.get(node.parentId) : undefined
      if (parent) parent.children.push(node)
      else roots.push(node)
    }
    const sort = (items: FolderTreeNode[]) => { items.sort((a, b) => a.sortOrder - b.sortOrder || a.name.localeCompare(b.name)); items.forEach((item) => sort(item.children)) }
    sort(roots)
    return roots
  })

  async function load(force = false): Promise<void> {
    if (loading.value || (loaded.value && !force)) return
    loading.value = true
    error.value = null
    try {
      ;[folders.value, deviceTypes.value, tags.value] = await Promise.all([api.listFolders(), api.listDeviceTypes(), api.listTags()])
      loaded.value = true
    } catch (cause) { error.value = localizedError(cause) } finally { loading.value = false }
  }

  function descendantIds(folderId: string): Set<string> {
    const ids = new Set([folderId])
    let changed = true
    while (changed) {
      changed = false
      for (const folder of folders.value) if (folder.parentId && ids.has(folder.parentId) && !ids.has(folder.id)) { ids.add(folder.id); changed = true }
    }
    return ids
  }

  return { folders, deviceTypes, tags, loading, loaded, error, folderById, tree, load, descendantIds }
})
