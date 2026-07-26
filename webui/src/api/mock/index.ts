import type { NodeInfo, SensorSample } from '@/types/api'
import { ApiError } from '../http'
import { findMockNode, generateMockHistory, mockNodes } from './fixtures'
import { mockSocket } from './mockSocket'

/** True when the app runs against the built-in mock api + mock socket. */
export const isMock = import.meta.env.VITE_MOCK === '1'

const NETWORK_DELAY_MS = 60

function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms))
}

/** Deep copy so store mutations never leak back into the mock "server" state. */
function cloneNode(node: NodeInfo): NodeInfo {
  const copy = structuredClone(node)
  for (const relay of copy.relays) relay.pending = false
  return copy
}

/** Mock implementations of the api functions (same signatures/semantics). */
export const mockApi = {
  async listNodes(): Promise<NodeInfo[]> {
    await delay(NETWORK_DELAY_MS)
    return mockNodes.map(cloneNode)
  },

  async getNode(nodeId: string): Promise<NodeInfo> {
    await delay(NETWORK_DELAY_MS)
    const node = findMockNode(nodeId)
    if (!node) throw new ApiError('NOT_FOUND', `Node ${nodeId} not found`, 404)
    return cloneNode(node)
  },

  /** Mirrors the real 202 semantics: resolves immediately without changing
   *  state; the RELAY_STATE "ack" arrives ~400ms later via mockSocket
   *  (never for the never-acking fixture node). */
  async sendRelayCommand(nodeId: string, channel: number, state: 'ON' | 'OFF'): Promise<void> {
    await delay(NETWORK_DELAY_MS)
    const node = findMockNode(nodeId)
    if (!node || !node.relays.some((r) => r.channel === channel)) {
      throw new ApiError('NOT_FOUND', `Relay ${channel} on ${nodeId} not found`, 404)
    }
    mockSocket.scheduleRelayAck(nodeId, channel, state)
  },

  async getSensorHistory(nodeId: string): Promise<SensorSample[]> {
    await delay(NETWORK_DELAY_MS)
    const node = findMockNode(nodeId)
    if (!node) throw new ApiError('NOT_FOUND', `Node ${nodeId} not found`, 404)
    if (!node.hasSensor) return []
    return generateMockHistory()
  },
}
