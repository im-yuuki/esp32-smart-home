import type { ServerEvent } from '@/types/events'
import {
  FLAPPING_NODE_ID,
  NEVER_ACK_NODE_ID,
  NORMAL_NODE_ID,
  findMockNode,
  round1,
} from './fixtures'

type EventHandler = (ev: ServerEvent) => void

const RELAY_ACK_DELAY_MS = 400
const SENSOR_TICK_MS = 5_000
const STATUS_FLIP_MS = 45_000
const CONNECT_DELAY_MS = 300

/**
 * Fake realtime event source used when VITE_MOCK=1. Feeds the exact same
 * `applyEvent` path as the real STOMP subscription — the stores are identical
 * in both modes. Mirrors the backend contract: envelope `ts` is epoch ms;
 * the sensor `data.ts` is epoch seconds (and is ignored by the store).
 */
class MockSocket {
  private handlers = new Set<EventHandler>()
  private timers: number[] = []
  private running = false

  connect(callbacks: { onConnect?: () => void } = {}): void {
    if (this.running) return
    this.running = true
    this.timers.push(
      window.setTimeout(() => {
        callbacks.onConnect?.()
        this.startTicks()
      }, CONNECT_DELAY_MS),
    )
  }

  disconnect(): void {
    this.running = false
    for (const t of this.timers) window.clearTimeout(t)
    this.timers = []
  }

  /** Subscribe to events; returns an unsubscribe function. */
  subscribe(handler: EventHandler): () => void {
    this.handlers.add(handler)
    return () => this.handlers.delete(handler)
  }

  /** Simulate the node executing a relay command (RELAY_STATE after ~400ms).
   *  The never-acking fixture node stays silent — the 5s timeout path fires. */
  scheduleRelayAck(nodeId: string, channel: number, state: 'ON' | 'OFF'): void {
    if (nodeId === NEVER_ACK_NODE_ID) return
    const node = findMockNode(nodeId)
    if (!node || !node.online) return
    this.timers.push(
      window.setTimeout(() => {
        const relay = node.relays.find((r) => r.channel === channel)
        if (relay) {
          relay.state = state
          relay.source = 'mqtt'
        }
        this.emit({
          type: 'RELAY_STATE',
          nodeId,
          channel,
          data: { state, source: 'mqtt' },
          ts: Date.now(),
        })
      }, RELAY_ACK_DELAY_MS),
    )
  }

  private emit(ev: ServerEvent): void {
    for (const handler of this.handlers) handler(ev)
  }

  private startTicks(): void {
    if (!this.running) return

    // Sensor random walk every 5s on the normal node.
    this.timers.push(
      window.setInterval(() => {
        const node = findMockNode(NORMAL_NODE_ID)
        if (!node || !node.sensor) return
        const temperature = round1(
          clamp(node.sensor.temperature + (Math.random() - 0.5) * 0.4, 20, 35),
        )
        const humidity = round1(clamp(node.sensor.humidity + (Math.random() - 0.5) * 1.5, 30, 95))
        const now = Date.now()
        node.sensor = { temperature, humidity, ts: now }
        this.emit({
          type: 'SENSOR_STATE',
          nodeId: NORMAL_NODE_ID,
          data: { temperature, humidity, ts: Math.floor(now / 1000) }, // data.ts = epoch SECONDS
          ts: now,
        })
      }, SENSOR_TICK_MS),
    )

    // Status flip every 45s on the flapping node.
    this.timers.push(
      window.setInterval(() => {
        const node = findMockNode(FLAPPING_NODE_ID)
        if (!node) return
        node.online = !node.online
        node.lastSeen = Date.now()
        this.emit({
          type: 'NODE_STATUS',
          nodeId: FLAPPING_NODE_ID,
          data: { online: node.online },
          ts: Date.now(),
        })
      }, STATUS_FLIP_MS),
    )
  }
}

function clamp(v: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, v))
}

export const mockSocket = new MockSocket()
