/** Room slug helpers with translations for the firmware's known room slugs. */

type Translate = (key: string) => string

export function roomLabel(slug: string, translate?: Translate): string {
  if (translate) {
    const key = `rooms.${slug}`
    const translated = translate(key)
    if (translated !== key) return translated
  }
  return slug
    .split(/[-_\s]+/)
    .filter(Boolean)
    .map((w) => w.charAt(0).toUpperCase() + w.slice(1))
    .join(' ')
}

/** Sort rooms by display label. */
export function roomCompare(a: string, b: string, locale = 'en', translate?: Translate): number {
  return roomLabel(a, translate).localeCompare(roomLabel(b, translate), locale)
}
