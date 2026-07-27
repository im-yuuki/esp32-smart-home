// Note: the design doc calls this enum ReconnectTimeMode; stompjs 7.3 exports
// it as ReconnectionTimeMode (the config FIELD is still `reconnectTimeMode`).
import { Client, ReconnectionTimeMode } from '@stomp/stompjs'
import SockJS from 'sockjs-client'
import { isMock } from '@/api/mock'
import { mockSocket } from '@/api/mock/mockSocket'
import { useNodesStore } from '@/stores/nodes'
import { useRealtimeStore } from '@/stores/realtime'
import type { ServerEvent } from '@/types/events'
import { getCsrfToken } from '@/api/http'
import { refreshCsrf } from '@/api/auth'
import { useAuthStore } from '@/stores/auth'

/** Module scope on purpose: the STOMP Client is non-serializable and must
 *  never enter Pinia state (Vue proxying breaks it). */
let client: Client | null = null
let mockUnsubscribe: (() => void) | null = null
let connectionGeneration = 0

/**
 * STOMP-over-SockJS lifecycle. Called once from App.vue.
 *
 * Uses stompjs >= 7.1 built-in exponential backoff (1s -> 30s cap) instead of
 * a hand-rolled loop. On every (re)connect: refetch /nodes (resync events
 * missed while down) and (re)subscribe /topic/events.
 */
export function useWebSocket() {
  const nodesStore = useNodesStore()
  const realtime = useRealtimeStore()
  const auth = useAuthStore()

  function handleFrameBody(body: string): void {
    try {
      nodesStore.applyEvent(JSON.parse(body) as ServerEvent)
    } catch (e) {
      console.error('[ws] unparseable event payload', e)
    }
  }

  async function connect(): Promise<void> {
    if (!auth.isAuthenticated) return
    if ((isMock && mockUnsubscribe) || (!isMock && client)) return
    const generation = ++connectionGeneration
    if (isMock) {
      connectMock(generation)
      return
    }
    const csrf = getCsrfToken() ?? await refreshCsrf()
    if (!auth.isAuthenticated || client || generation !== connectionGeneration) return

    realtime.setConnecting()
    const ownedClient = new Client({
      // Factory, not brokerURL: a SockJS object is single-use — one fresh
      // instance per reconnect attempt. Absolute URL: SockJS is picky about
      // relative ones. Same-origin `/ws` works in dev (Vite proxy) and prod (nginx).
      webSocketFactory: () => new SockJS(`${location.origin}/ws`),
      connectHeaders: { [csrf.headerName]: csrf.token },
      reconnectDelay: 1_000,
      maxReconnectDelay: 30_000,
      reconnectTimeMode: ReconnectionTimeMode.EXPONENTIAL, // 1s, 2s, 4s ... 30s cap
      heartbeatIncoming: 10_000,
      heartbeatOutgoing: 10_000,
      onConnect: () => {
        if (client !== ownedClient || generation !== connectionGeneration) return
        realtime.setConnected()
        // RESYNC: refetch full state after every (re)connect — events were
        // missed while the connection was down.
        void nodesStore.fetchNodes()
        ownedClient.subscribe('/user/queue/events', (frame) => {
          if (client === ownedClient && generation === connectionGeneration) handleFrameBody(frame.body)
        })
      },
      onWebSocketClose: () => {
        if (client === ownedClient && generation === connectionGeneration && auth.isAuthenticated) {
          realtime.setReconnecting()
        }
      },
      onStompError: (frame) => {
        if (client !== ownedClient || generation !== connectionGeneration) return
        console.error('[ws] STOMP error:', frame.headers['message'], frame.body)
      },
    })
    client = ownedClient
    ownedClient.activate()
  }

  /** Mock branch: subscribes mockSocket's emitter — same applyEvent path,
   *  stores are identical in both modes. */
  function connectMock(generation: number): void {
    if (mockUnsubscribe) return
    realtime.setConnecting()
    mockUnsubscribe = mockSocket.subscribe((ev) => {
      if (generation === connectionGeneration && auth.isAuthenticated) nodesStore.applyEvent(ev)
    })
    mockSocket.connect({
      onConnect: () => {
        if (generation !== connectionGeneration || !auth.isAuthenticated) return
        realtime.setConnected()
        void nodesStore.fetchNodes()
      },
    })
  }

  /** Exposed for tests/teardown. */
  async function disconnect(): Promise<void> {
    connectionGeneration++
    if (isMock) {
      mockUnsubscribe?.()
      mockUnsubscribe = null
      mockSocket.disconnect()
      realtime.setDisconnected()
      return
    }
    if (client) {
      const c = client
      client = null
      await c.deactivate()
    }
    realtime.setDisconnected()
  }

  return { connect, disconnect }
}
