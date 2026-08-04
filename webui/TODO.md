# WebUI Redesign Checklist

## Completed

- [x] Replace Nuxt UI and the interim PrimeVue implementation with source-owned shadcn-vue components.
- [x] Remove PrimeVue packages, services, theme presets, license variables, Docker arguments, and Compose arguments.
- [x] Add `components.json` using the `new-york` style, neutral variables, Lucide icons, and existing aliases.
- [x] Add Tailwind merge, class variance authority, Reka UI, Lucide Vue, and Sonner dependencies.
- [x] Add shared Button, Input, Label, Card, Badge, Alert, Skeleton, Checkbox, and Switch primitives.
- [x] Keep the local icon adapter for persisted backend `i-lucide-*` values.
- [x] Keep notifications behind an application composable.
- [x] Keep `motion-v` for route, navigation drawer, and bulk-control transitions.
- [x] Implement persistent light/dark mode with pre-mount initialization.
- [x] Apply a strict black/white/grayscale token system.
- [x] Remove green accents, decorative gradients, hero cards, and long instructional descriptions.
- [x] Keep focus indicators and reduced-motion CSS.

## Layout and UX

- [x] Redesign the shell as a compact black command bar, persistent facility rail, content workspace, and activity rail.
- [x] Add a skip link and keep one main landmark.
- [x] Keep desktop sidebar collapse and per-user folder expansion persistence.
- [x] Add a mobile modal navigation drawer with Escape and backdrop dismissal.
- [x] Keep accent-insensitive facility/device search and permission-aware navigation.
- [x] Add a first-class `/logs` audit workspace while retaining the lightweight activity rail.
- [x] Redesign authentication as compact labeled forms without hero content.
- [x] Redesign facility browsing with a compact context header and map/list controls.
- [x] Change map markers from direct commands to selection plus a focused device inspector.
- [x] Keep list-mode device controls, tags, telemetry summary, and detail navigation.
- [x] Redesign bulk control as a right-side review drawer with mandatory preview before confirmation.
- [x] Redesign node details around identity, commands, current telemetry, and history.
- [x] Add a textual telemetry summary for assistive technology.
- [x] Redesign administration as compact folder, node, capability, placement, role, and membership workspaces.
- [x] Replace folder deletion with an in-app confirmation dialog.
- [x] Keep mobile layouts free of horizontal page overflow.

## Preserved Behavior

- [x] Preserve route parameters, history fallback, root redirect, authentication guards, and safe redirects.
- [x] Preserve forced password changes, CSRF handling, and centralized unauthorized handling.
- [x] Preserve permission gates for nodes, telemetry, administration, and audit.
- [x] Preserve one authenticated WebSocket connection and reconnect resync.
- [x] Preserve shared node state across map, list, and detail screens.
- [x] Preserve non-optimistic relay commands, pending state, timeout, revert, and offline abort.
- [x] Preserve map placement, bulk idempotency, preview/execute results, and batch IDs.
- [x] Preserve sensor history, live append, timestamp deduplication, locale updates, and pinch zoom.
- [x] Preserve folder, node, capability, placement, role, and membership administration.
- [x] Preserve English/Vietnamese localization and mock-mode deterministic scenarios.

## Documentation and Validation

- [x] Update root and WebUI README stack references.
- [x] Mark the Nuxt UI ADR as superseded and add the shadcn-vue ADR.
- [x] Regenerate `package-lock.json` and confirm old UI packages are absent.
- [x] Pass `npm run type-check`.
- [x] Pass `npm run build-only` and `npm run build`.
- [x] Pass `git diff --check`.
- [x] Smoke-test facility navigation, marker inspection, bulk drawer, audit route, admin, and mobile overflow in mock mode.
- [x] Confirm no browser console errors during smoke testing.

## Follow-up

- [ ] Add automated browser and visual-regression tests when the project adopts a frontend test runner.
- [ ] Review the existing large-chunk build warning and define a bundle budget.
