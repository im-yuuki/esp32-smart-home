import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import { NODE_CONTROL, type NodeInfo, type RelayState } from '@/types/api'
import type { ServerEvent } from '@/types/events'
import { listNodes, sendRelayCommand as postRelayCommand } from '@/api/nodes'
import { localizedError } from '@/i18n/errors'
import { i18n } from '@/i18n'
import { roomCompare } from '@/utils/rooms'
import { useRealtimeStore } from './realtime'

const COMMAND_TIMEOUT_MS = 5_000

interface PendingEntry {
  desired: 'ON' | 'OFF'
  prevState: RelayState
  timer: number
}

/**
 * NON-reactive module scope: timer ids must not live in reactive state.
 * Key = `${nodeId}:${channel}`. The reactive `pending` flag lives on the
 * RelayChannel objects inside the store's node map.
 */
const pendingMap = new Map<string, PendingEntry>()
const eventRevisions = new Map<string, number>()

const keyOf = (nodeId: string, channel: number) => `${nodeId}:${channel}`

function parseKey(key: string): { nodeId: string; channel: number } {
  const idx = key.lastIndexOf(':')
  return { nodeId: key.slice(0, idx), channel: Number(key.slice(idx + 1)) }
}

/**
 * Node state + the relay pending/timeout state machine.
 *
 * The 5s timer lives HERE, not in components: the resolving RELAY_STATE event
 * lands in `applyEvent`, the machine must survive route navigation, and the
 * same relay renders in two places (dashboard card + detail page).
 *
 * Transitions for key `nodeId:channel`:
 *   IDLE    --toggle (node.online && realtime.connected && !pending)--> PENDING
 *   PENDING --RELAY_STATE event--> IDLE   (event state wins, even if != desired)
 *   PENDING --5s timeout--------> IDLE   (revert to prevState + error toast)
 *   PENDING --NODE_STATUS offline-> IDLE  (abort early, same revert + toast)
 * The switch never flips optimistically — POST 202 = accepted, not executed.
 */
export const useNodesStore = defineStore('nodes', () => {
  const nodes = ref(new Map<string, NodeInfo>())
  const loading = ref(false)
  const loaded = ref(false)
  const loadError = ref<string | null>(null)
  const selectedGroupId = ref<number | null>(null)

  const realtime = useRealtimeStore()

  // Nuxt UI toast — auto-imported by the @nuxt/ui vite plugin. Safe outside
  // component setup in standalone Vue mode: toast state is module-scoped
  // (useState stub), and this store is first instantiated from App.vue setup.
  // (Fallback if this ever breaks: a tiny mitt-style toastBus consumed in App.vue.)
  const toast = useToast()

  // ------------------------------------------------------------------ getters

  const nodesByRoom = computed<Array<[string, NodeInfo[]]>>(() => {
    const rooms = new Map<string, NodeInfo[]>()
    for (const node of nodes.value.values()) {
      if (selectedGroupId.value !== null && !node.groupIds.includes(selectedGroupId.value)) continue
      const list = rooms.get(node.room)
      if (list) list.push(node)
      else rooms.set(node.room, [node])
    }
    for (const list of rooms.values()) {
      list.sort((a, b) => a.nodeId.localeCompare(b.nodeId))
    }
    return [...rooms.entries()].sort((a, b) =>
      roomCompare(a[0], b[0], i18n.global.locale.value, (key) => i18n.global.t(key)),
    )
  })

  function nodeById(nodeId: string): NodeInfo | undefined {
    return nodes.value.get(nodeId)
  }

  function findRelay(nodeId: string, channel: number) {
    return nodes.value.get(nodeId)?.relays.find((r) => r.channel === channel)
  }

  // ------------------------------------------------------------------ actions

  let fetchInFlight: Promise<void> | null = null
  let fetchGeneration = 0

  /** GET /nodes and replace the map. Also called after every WS (re)connect —
   *  resync covers events missed while the connection was down. */
  function fetchNodes(): Promise<void> {
    if (fetchInFlight) return fetchInFlight
    loading.value = true
    const generation = fetchGeneration
    const revisionsAtStart = new Map(eventRevisions)
    fetchInFlight = (async () => {
      try {
        const list = await listNodes()
        if (generation !== fetchGeneration) return
        const map = new Map<string, NodeInfo>()
        for (const node of list) {
          const existing = nodes.value.get(node.nodeId)
          if (existing && (eventRevisions.get(node.nodeId) ?? 0) !== (revisionsAtStart.get(node.nodeId) ?? 0)) {
            node.online = existing.online
            node.lastSeen = existing.lastSeen
            node.sensor = existing.sensor
            for (const relay of node.relays) {
              const currentRelay = existing.relays.find((item) => item.channel === relay.channel)
              if (currentRelay) {
                relay.state = currentRelay.state
                relay.source = currentRelay.source
                relay.pending = currentRelay.pending
              }
            }
          }
          map.set(node.nodeId, node)
        }
        // Preserve the pending flag for commands still in flight (the fetched
        // snapshot knows nothing about our local machine).
        for (const key of pendingMap.keys()) {
          const { nodeId, channel } = parseKey(key)
          const relay = map.get(nodeId)?.relays.find((r) => r.channel === channel)
          if (relay) relay.pending = true
        }
        nodes.value = map
        loaded.value = true
        loadError.value = null
      } catch (e) {
        if (generation !== fetchGeneration) return
        loadError.value = localizedError(e)
      } finally {
        if (generation === fetchGeneration) {
          loading.value = false
          fetchInFlight = null
        }
      }
    })()
    return fetchInFlight
  }

  /** IDLE -> PENDING. Guarded: no-op unless online + WS connected + not pending. */
  async function sendRelayCommand(
    nodeId: string,
    channel: number,
    desired: 'ON' | 'OFF',
  ): Promise<void> {
    const key = keyOf(nodeId, channel)
    const node = nodes.value.get(nodeId)
    const relay = node?.relays.find((r) => r.channel === channel)
    if (!node || !relay) return
    if (!node.permissions.includes(NODE_CONTROL)
        || !node.online || !realtime.isConnected || pendingMap.has(key)) return

    const prevState = relay.state
    relay.pending = true
    const timer = window.setTimeout(() => onCommandTimeout(key), COMMAND_TIMEOUT_MS)
    pendingMap.set(key, { desired, prevState, timer })

    try {
      await postRelayCommand(nodeId, channel, desired)
      // 202 accepted — nothing else to do; RELAY_STATE (or the timeout) resolves.
    } catch (e) {
      // The POST itself failed (network/5xx): abort immediately.
      const entry = pendingMap.get(key)
      if (entry) {
        window.clearTimeout(entry.timer)
        pendingMap.delete(key)
      }
      relay.pending = false
      relay.state = prevState
      toast.add({
        title: i18n.global.t('relay.commandFailed'),
        description: localizedError(e),
        color: 'error',
        icon: 'i-lucide-circle-alert',
      })
    }
  }

  /** PENDING -> IDLE via 5s timeout: visual revert + toast. */
  function onCommandTimeout(key: string): void {
    const entry = pendingMap.get(key)
    if (!entry) return
    pendingMap.delete(key)
    const { nodeId, channel } = parseKey(key)
    const relay = findRelay(nodeId, channel)
    if (relay) {
      relay.state = entry.prevState
      relay.pending = false
    }
    toast.add({
      title: i18n.global.t('relay.notResponding'),
      description: i18n.global.t('relay.timeout', { nodeId, channel, seconds: COMMAND_TIMEOUT_MS / 1000 }),
      color: 'error',
      icon: 'i-lucide-wifi-off',
    })
  }

  /** PENDING -> IDLE early abort (node went offline while a command was in flight). */
  function abortPendingForNode(nodeId: string): void {
    for (const [key, entry] of [...pendingMap.entries()]) {
      const parsed = parseKey(key)
      if (parsed.nodeId !== nodeId) continue
      window.clearTimeout(entry.timer)
      pendingMap.delete(key)
      const relay = findRelay(nodeId, parsed.channel)
      if (relay) {
        relay.state = entry.prevState
        relay.pending = false
      }
      toast.add({
        title: i18n.global.t('relay.notResponding'),
        description: i18n.global.t('relay.offlineBeforeConfirmation', { nodeId, channel: parsed.channel }),
        color: 'error',
        icon: 'i-lucide-wifi-off',
      })
    }
  }

  /** Single dispatch entry point for realtime events (real STOMP or mockSocket). */
  function applyEvent(ev: ServerEvent): void {
    eventRevisions.set(ev.nodeId, (eventRevisions.get(ev.nodeId) ?? 0) + 1)
    const node = nodes.value.get(ev.nodeId)
    if (!node) {
      // A node that self-discovered after page load — full refetch (Phase 1 simplicity).
      void fetchNodes()
      return
    }

    switch (ev.type) {
      case 'RELAY_STATE': {
        const relay = node.relays.find((r) => r.channel === ev.channel)
        if (!relay) {
          void fetchNodes() // capability set changed under us
          return
        }
        const key = keyOf(ev.nodeId, ev.channel)
        const entry = pendingMap.get(key)
        if (entry) {
          window.clearTimeout(entry.timer)
          pendingMap.delete(key)
        }
        // The state topic is the source of truth — apply the event state even
        // if it differs from `desired` (e.g. a physical-button race).
        relay.state = ev.data.state
        relay.source = ev.data.source
        relay.pending = false
        break
      }
      case 'SENSOR_STATE': {
        // Envelope ts is epoch ms; data.ts (epoch seconds) intentionally ignored.
        node.sensor = {
          temperature: ev.data.temperature,
          humidity: ev.data.humidity,
          ts: ev.ts,
        }
        break
      }
      case 'NODE_STATUS': {
        node.online = ev.data.online
        node.lastSeen = ev.ts
        if (!ev.data.online) {
          // Fail fast: a pending command on an offline node can never be confirmed.
          abortPendingForNode(ev.nodeId)
        }
        // Offline does not wipe relay states (retained topics restore on reconnect).
        break
      }
    }
  }

  function can(node: NodeInfo, permission: string): boolean {
    return node.permissions.includes(permission)
  }

  function reset(): void {
    fetchGeneration++
    for (const entry of pendingMap.values()) window.clearTimeout(entry.timer)
    pendingMap.clear()
    eventRevisions.clear()
    nodes.value = new Map()
    loading.value = false
    loaded.value = false
    loadError.value = null
    selectedGroupId.value = null
    fetchInFlight = null
  }

  return {
    nodes,
    loading,
    loaded,
    loadError,
    selectedGroupId,
    nodesByRoom,
    nodeById,
    fetchNodes,
    sendRelayCommand,
    applyEvent,
    can,
    reset,
  }
})
