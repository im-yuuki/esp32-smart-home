/**
 * STOMP /topic/events payloads — mirrors the backend's
 * EventMessage(type, nodeId, channel, data, ts) record.
 * The envelope `ts` is epoch **milliseconds** (set by the server).
 */

export interface RelayStateEvent {
  type: 'RELAY_STATE'
  nodeId: string
  channel: number
  data: {
    state: 'ON' | 'OFF'
    source?: string // 'mqtt' | 'button' | 'boot'
  }
  ts: number // epoch ms
}

export interface SensorStateEvent {
  type: 'SENSOR_STATE'
  nodeId: string
  channel?: number | null
  data: {
    temperature: number
    humidity: number
    /** Node-reported timestamp in epoch SECONDS — intentionally ignored;
     *  the envelope `ts` (epoch ms) is used instead. */
    ts?: number
  }
  ts: number // epoch ms
}

export interface NodeStatusEvent {
  type: 'NODE_STATUS'
  nodeId: string
  channel?: number | null
  data: { online: boolean }
  ts: number // epoch ms
}

export type ServerEvent = RelayStateEvent | SensorStateEvent | NodeStatusEvent
