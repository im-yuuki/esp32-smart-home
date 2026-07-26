/** Timestamp normalization helpers (everything in the app is epoch ms) + formatting. */

/** ISO-8601 string -> epoch ms (null when absent/unparseable). */
export function isoToMs(iso: string | null | undefined): number | null {
  if (!iso) return null
  const ms = Date.parse(iso)
  return Number.isNaN(ms) ? null : ms
}

/** Epoch seconds (MQTT payloads) -> epoch ms. */
export function secondsToMs(seconds: number): number {
  return Math.round(seconds * 1000)
}

/** Relative "updated Xs ago" formatter. Pass a ticking `nowMs` for live updates. */
export function relativeTime(tsMs: number, nowMs: number = Date.now()): string {
  const diffS = Math.max(0, Math.floor((nowMs - tsMs) / 1000))
  if (diffS < 5) return 'just now'
  if (diffS < 60) return `${diffS}s ago`
  const m = Math.floor(diffS / 60)
  if (m < 60) return `${m}m ago`
  const h = Math.floor(m / 60)
  if (h < 24) return `${h}h ago`
  const d = Math.floor(h / 24)
  return `${d}d ago`
}

/** Absolute local date-time for detail views. */
export function absoluteTime(tsMs: number): string {
  return new Date(tsMs).toLocaleString()
}
