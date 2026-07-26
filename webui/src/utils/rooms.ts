/** Room slug helpers: `phong-khach` -> "Phong Khach". */

/** Static overrides for slugs whose display name isn't a simple title-case. */
const ROOM_LABEL_OVERRIDES: Record<string, string> = {
  // 'phong-khach': 'Phòng khách',
}

export function roomLabel(slug: string): string {
  const override = ROOM_LABEL_OVERRIDES[slug]
  if (override) return override
  return slug
    .split(/[-_\s]+/)
    .filter(Boolean)
    .map((w) => w.charAt(0).toUpperCase() + w.slice(1))
    .join(' ')
}

/** Sort rooms by display label. */
export function roomCompare(a: string, b: string): number {
  return roomLabel(a).localeCompare(roomLabel(b))
}
