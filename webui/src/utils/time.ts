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

/** Locale-aware relative formatter. Pass a ticking `nowMs` for live updates. */
export function relativeTime(tsMs: number, nowMs: number = Date.now(), locale = 'en'): string {
  const diffS = Math.max(0, Math.floor((nowMs - tsMs) / 1000))
  const formatter = new Intl.RelativeTimeFormat(locale, { numeric: 'auto' })
  if (diffS < 5) return formatter.format(0, 'second')
  if (diffS < 60) return formatter.format(-diffS, 'second')
  const m = Math.floor(diffS / 60)
  if (m < 60) return formatter.format(-m, 'minute')
  const h = Math.floor(m / 60)
  if (h < 24) return formatter.format(-h, 'hour')
  const d = Math.floor(h / 24)
  return formatter.format(-d, 'day')
}

/** Absolute local date-time for detail views. */
export function absoluteTime(tsMs: number, locale = 'en'): string {
  return new Intl.DateTimeFormat(locale, {
    dateStyle: 'medium',
    timeStyle: 'short',
  }).format(tsMs)
}
