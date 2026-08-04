# ADR-0003: Nuxt UI v4 as the web UI component library

**Status:** superseded by ADR-0006 (2026-08-05)

## Context
The roadmap suggests PrimeVue or Naive UI ("pick one and use it consistently"). The user asked for Nuxt UI instead.

## Decision
**Nuxt UI v4** running standalone in Vue 3 + Vite (no Nuxt): `@nuxt/ui/vite` plugin in `vite.config.ts`, `@nuxt/ui/vue-plugin` in `main.ts`, CSS `@import "tailwindcss"; @import "@nuxt/ui";` (Tailwind v4 — no `tailwind.config.js`). Root component wraps everything in `<UApp>` (required for toasts/tooltips). Generated `auto-imports.d.ts` / `components.d.ts` are gitignored and referenced from `tsconfig.app.json`.

## Consequences
- The roadmap's "pick one, stay consistent" rule is satisfied — Nuxt UI everywhere, ECharts for charts.
- v4 semantic color props (`color="error"`), not v3 palette names; Tailwind v3-era config snippets do not apply.
