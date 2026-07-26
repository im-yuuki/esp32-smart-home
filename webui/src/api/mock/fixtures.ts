import type { NodeInfo, SensorSample } from '@/types/api'

/** Fixture node that never acks relay commands — deterministically exercises
 *  the 5s pending-timeout path (spinner -> revert -> "Device not responding"). */
export const NEVER_ACK_NODE_ID = 'esp32s3-badbad'

/** Fixture node whose online status flips periodically (exercises dimming/abort). */
export const FLAPPING_NODE_ID = 'esp32s3-c0ffee'

/** The "normal" node: acks commands, has a live sensor. */
export const NORMAL_NODE_ID = 'esp32s3-aabbcc'

/**
 * Mutable mock "server-side" state. mockSocket mutates it when it emits events
 * so a later listNodes() resync (e.g. after reconnect) stays consistent.
 */
export const mockNodes: NodeInfo[] = [
  {
    nodeId: NORMAL_NODE_ID,
    room: 'phong-khach',
    fwVersion: '1.0.0',
    ip: '192.168.1.51',
    online: true,
    lastSeen: Date.now(),
    relays: [
      { channel: 1, name: 'Den tran', state: 'OFF', source: 'boot', pending: false },
      { channel: 2, name: 'Den ban', state: 'ON', source: 'mqtt', pending: false },
    ],
    hasSensor: true,
    sensorMeta: { kind: 'temperature_humidity', model: 'SHT31', intervalS: 30 },
    sensor: { temperature: 28.5, humidity: 65.2, ts: Date.now() },
  },
  {
    nodeId: FLAPPING_NODE_ID,
    room: 'phong-ngu',
    fwVersion: '1.0.0',
    ip: '192.168.1.52',
    online: false,
    lastSeen: Date.now() - 5 * 60_000,
    relays: [{ channel: 1, name: 'Den ngu', state: 'OFF', source: 'boot', pending: false }],
    hasSensor: false,
    sensor: null,
  },
  {
    nodeId: NEVER_ACK_NODE_ID,
    room: 'phong-ngu',
    fwVersion: '0.9.0',
    ip: '192.168.1.53',
    online: true,
    lastSeen: Date.now(),
    relays: [{ channel: 1, name: 'Quat tran', state: 'OFF', source: 'boot', pending: false }],
    hasSensor: false,
    sensor: null,
  },
]

export function findMockNode(nodeId: string): NodeInfo | undefined {
  return mockNodes.find((n) => n.nodeId === nodeId)
}

/** 24h of synthetic sensor history: daily sine + noise, 30s interval (~2880 pts). */
export function generateMockHistory(nowMs: number = Date.now()): SensorSample[] {
  const samples: SensorSample[] = []
  const intervalMs = 30_000
  const count = Math.floor((24 * 3600 * 1000) / intervalMs)
  const dayMs = 24 * 3600 * 1000
  for (let i = count; i >= 1; i--) {
    const ts = nowMs - i * intervalMs
    const phase = (2 * Math.PI * (ts % dayMs)) / dayMs
    samples.push({
      ts,
      temperature: round1(27.5 + 3 * Math.sin(phase) + (Math.random() - 0.5) * 0.6),
      humidity: round1(65 - 8 * Math.sin(phase) + (Math.random() - 0.5) * 2),
    })
  }
  return samples
}

export function round1(v: number): number {
  return Math.round(v * 10) / 10
}
