# ADR-0006: shadcn-vue for the web UI

**Status:** accepted (2026-08-05, user decision)

## Context

The WebUI needs a compact, task-focused operator experience with a strict monochrome design and direct ownership of component behavior and styling.

## Decision

Use shadcn-vue conventions with source-owned Vue components under `src/components/ui/`, Tailwind CSS v4, Reka UI accessibility primitives, Lucide icons, and CSS-variable light/dark themes. Use Motion for Vue only for functional transitions. ECharts remains the chart renderer.

## Consequences

- UI components are application source, not a runtime component-library dependency.
- The design system is defined by local neutral tokens and can be changed without library theme APIs.
- Persisted backend `i-lucide-*` names continue through the local `AppIcon` adapter.
- New primitives should be added selectively through `components.json` and reviewed like application code.
