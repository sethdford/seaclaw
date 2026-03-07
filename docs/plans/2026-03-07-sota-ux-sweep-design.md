# SOTA UX Sweep — Design Document

**Date:** 2026-03-07
**Status:** Approved
**Scope:** All 14 dashboard views + 7 new components

## Problem

The seaclaw design system primitives (55 components, design tokens, spring physics, glass
tiers) are strong. But the views that compose them are not — dense lists without sort/filter,
raw JSON dumps, CSS bar-chart hacks instead of real data viz, missing loading/empty states,
read-only cards with no actions. The chat view is SOTA after Project Meridian; every other
view is two tiers below.

## Decision

Systematic sweep organized in 4 waves. Each wave builds 1-3 missing primitives then
immediately applies them to 3-4 views. Ordered by user-facing impact: data visualization
first, interactive tables second, forms third, polish last.

## Benchmarks

Target: **Linear/Raycast/Vercel tier** — opinionated tools for power users.

| Property        | Material 3        | Fidelity         | Our Target                                        |
| --------------- | ----------------- | ---------------- | ------------------------------------------------- |
| Density         | Low (touch-first) | High (cramped)   | High with hierarchy                               |
| Data viz        | None built-in     | Basic tables     | Canvas charts + data-viz tokens                   |
| Structured data | None              | Table dumps      | Collapsible JSON tree viewer                      |
| Tables          | Anemic DataTable  | Dense but static | Sort, filter, paginate, row actions, keyboard nav |
| Motion          | Basic ease curves | None             | Spring physics, stagger, animated numbers         |
| Theming         | Dynamic color     | Fixed brand      | Token-pure `--sc-*`, dark/light, glass tiers      |

## Architecture

### New Components (7)

#### Wave 1: sc-chart

Canvas-based chart via Chart.js (dynamic import from CDN, ~60KB gzip). Types: bar, line,
area, doughnut. Consumes `--sc-chart-*` CSS custom properties mapped from
`data-viz.tokens.json`. Respects `prefers-reduced-motion`. Labels use `--sc-font`. Fires
`sc-chart-hover` events. Responsive via `ResizeObserver`.

Properties:

- `type`: `"bar" | "line" | "area" | "doughnut"`
- `data`: `ChartData` (labels + datasets array)
- `options`: optional overrides
- `height`: number (default 200)

#### Wave 1: sc-json-viewer

Collapsible tree for structured data. Syntax-highlighted keys/values. Copy node to clipboard.
Collapsed past depth 2 by default. Mono font via `--sc-font-mono`. Arrow-key navigation.

Properties:

- `data`: `unknown` (any JSON-serializable value)
- `expanded-depth`: number (default 2)
- `root-label`: string (optional)

#### Wave 2: sc-data-table-v2

Full-featured data table replacing existing `sc-data-table`. Adds: column sorting (click
header, asc/desc/none cycle), text filter per filterable column, pagination bar (10/25/50
page sizes), row click handler (`sc-row-click`), row actions slot (kebab menu), keyboard
navigation, sticky header, empty state slot, responsive card-per-row on mobile.

Column definition extends existing:

```typescript
interface DataTableColumnV2 {
  key: string;
  label: string;
  align?: "left" | "center" | "right";
  width?: string;
  sortable?: boolean;
  filterable?: boolean;
  render?: (value: unknown, row: Record<string, unknown>) => TemplateResult;
}
```

#### Wave 2: sc-pagination

Standalone pagination. Page numbers, prev/next, "Showing X–Y of Z" label. Fires
`sc-page-change` with `{ page, pageSize }`.

Properties:

- `total`: number
- `page`: number
- `page-size`: number
- `page-sizes`: number[] (default [10, 25, 50])

#### Wave 3: sc-combobox

Autocomplete input with dropdown list. Filters options as user types. Keyboard navigable
(arrow keys, Enter, Escape). Constrained or free-text mode. Fires `sc-combobox-change`.

Properties:

- `options`: `{ value: string; label: string }[]`
- `value`: string
- `placeholder`: string
- `free-text`: boolean (default false)

#### Wave 3: sc-checkbox

Standard checkbox. Supports indeterminate, label, disabled, error. Matches `sc-switch` /
`sc-radio` API surface.

Properties:

- `checked`: boolean
- `indeterminate`: boolean
- `disabled`: boolean
- `label`: string
- `error`: string

#### Wave 3: sc-form-group

Thin validation coordinator for child inputs. Inline validation messages. Dirty/pristine
tracking. Fires `sc-form-submit` with validated data on explicit submit.

Properties:

- `dirty`: boolean (readonly)
- `valid`: boolean (readonly)

Methods:

- `validate(): boolean`
- `reset(): void`

## Wave 1: "See Everything" — Data Viz + Usage + Logs + Overview

### Usage View — Full Rewrite

- Top: 3 stat cards (tokens, cost, requests) with inline trend sparklines
- Middle: `sc-chart type="area"` for token usage over time
- Time range toggle via `sc-segmented-control` (24h / 7d / 30d)
- Bottom: `sc-chart type="bar"` for cost breakdown by provider (horizontal, categorical
  palette from data-viz tokens)
- Export button (CSV/JSON download)

### Logs View — Full Rewrite

- Full-height log stream (flex: 1, no fixed 400px)
- Payload rendered via `sc-json-viewer` (collapsed, click to expand)
- Log level filter chips via `sc-segmented-control` (All / Chat / Tool / Error / Health)
- Pause/resume toggle for live streaming
- Relative timestamps ("2s ago") with absolute on hover
- Sticky controls header

### Overview View — Targeted Upgrade

- `sc-chart type="line"` in "Recent Sessions" showing activity volume
- Session table rows become clickable cards navigating to `#chat:sessionKey`
- `sc-chart type="doughnut"` for channel health (configured vs total)

## Wave 2: "Find & Act On Anything" — Interactive Tables

### Tools View — Full Rewrite

- Card grid replaced with `sc-data-table-v2` (Name, Description, Param count)
- Row click expands inline `sc-json-viewer` for parameter schema
- Single count stat + search replaces redundant stat cards

### Channels View — Full Rewrite

- Card grid replaced with `sc-data-table-v2` (Name, Status, Health, Last Active)
- Row click opens `sc-sheet` with full config
- "Configure" action button per row
- Filter chips: All / Configured / Unconfigured

### Nodes View — Full Rewrite

- Flat cards replaced with `sc-data-table-v2` (Node ID, Status, WS Count, Uptime)
- Row click opens detail sheet
- Refresh with last-updated timestamp

### Agents View — Targeted Upgrade

- Sessions list uses `sc-data-table-v2` with sort/filter
- Clickable rows navigate to `#chat:sessionKey`
- `sc-chart type="bar"` for sessions-per-day

## Wave 3: "Control Everything" — Forms & Config

### Config View — Full Rewrite

- Grouped form sections via `sc-form-group`
- Inline validation (temperature range, required fields, URL format)
- Provider/model via `sc-combobox`
- "Unsaved changes" banner with Save / Revert
- Raw JSON mode: `sc-code-block` with highlighting
- Diff toggle (changed vs saved)

### Security View — Full Rewrite

- Native `<select>` replaced with `sc-select`
- Visual risk indicator (green/amber/coral) tied to autonomy level
- Editable sandbox/network sections via `sc-switch`
- "Security Score" stat card
- Device pairing: show name, date, unpair action

### Automations View — Targeted Upgrade

- Modal validation via `sc-form-group`
- Run history in `sc-data-table-v2` with pagination
- `sc-chart type="line"` for run success/failure rate

## Wave 4: "Delight Everywhere" — Polish

### Models View — Full Rewrite

- Fix heading: `sc-page-hero` + `sc-section-header`
- "Set as default" action on provider cards
- `sc-chart type="doughnut"` for request distribution by provider
- Inline model/provider editing via `sc-combobox`

### Voice View — Targeted Upgrade

- Fix duration stat
- Conversation export
- Replace `sc-message-stream` with `sc-message-thread`
- Session management

### Skills View — Targeted Upgrade

- `sc-json-viewer` for parameter display
- Tag filtering for installed skills
- URL install validation + progress

### Cross-View Consistency Pass

- Stagger animations on every view load
- Skeleton shapes matching content
- Empty states with actionable CTAs
- Responsive breakpoints at 3 tiers (sm/md/lg)
- Trend indicators on all stat cards
- "No results" states on all lists/tables
- `view-transition-name` CSS per view for route transitions

## Deliverables Summary

| Wave | New Components                          | Views Rewritten        | Views Upgraded              |
| ---- | --------------------------------------- | ---------------------- | --------------------------- |
| 1    | sc-chart, sc-json-viewer                | Usage, Logs            | Overview                    |
| 2    | sc-data-table-v2, sc-pagination         | Tools, Channels, Nodes | Agents                      |
| 3    | sc-combobox, sc-checkbox, sc-form-group | Config, Security       | Automations                 |
| 4    | —                                       | Models                 | Voice, Skills + consistency |

**Total: 7 new components, 8 view rewrites, 6 view upgrades, 1 cross-view consistency pass.**
