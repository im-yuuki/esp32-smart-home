import { http } from './http'
import { isMock, mockApi } from './mock'
import type { SensorReadingDto, SensorSample } from '@/types/api'
import { isoToMs } from '@/utils/time'

export interface HistoryRange {
  /** epoch ms — converted to ISO-8601 `from` param. Omit for the server's 24h default. */
  fromMs?: number
  /** epoch ms — converted to ISO-8601 `to` param. Omit for the server's default (now). */
  toMs?: number
}

/**
 * GET /nodes/{nodeId}/sensors/history — params are `from`/`to` ISO-8601 instants
 * (NOT `?hours=`). Omitting both yields the backend's now-24h .. now default.
 * Row `ts` arrives as an ISO-8601 string and is normalized to epoch ms here.
 */
export async function getSensorHistory(
  nodeId: string,
  range: HistoryRange = {},
): Promise<SensorSample[]> {
  if (isMock) return mockApi.getSensorHistory(nodeId)

  const params: Record<string, string> = {}
  if (range.fromMs !== undefined) params.from = new Date(range.fromMs).toISOString()
  if (range.toMs !== undefined) params.to = new Date(range.toMs).toISOString()

  const res = await http.get<SensorReadingDto[]>(
    `/nodes/${encodeURIComponent(nodeId)}/sensors/history`,
    { params },
  )

  const samples: SensorSample[] = []
  for (const row of res.data) {
    const ts = isoToMs(row.ts)
    if (ts === null || row.temperature === null || row.humidity === null) continue
    samples.push({ ts, temperature: row.temperature, humidity: row.humidity })
  }
  return samples
}
