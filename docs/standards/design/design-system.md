---
title: Human Design System
---
# Human Design System

Overview of the design system philosophy, tokens, and usage rules for all Human UI surfaces (web dashboard, website, native apps, CLI/TUI).

---

## Philosophy

Human's design system prioritizes:

- **Consistency** — One canonical source of truth (`design-tokens/`) drives all platforms
- **Accessibility** — WCAG 2.1 AA minimum, prefers-reduced-motion, semantic HTML
- **Lightweight** — No external font CDNs, minimal runtime overhead, single binary mentality
- **Portability** — Same tokens, same philosophy across CSS, Swift, Kotlin, and C

---

## Color System

### Primary Accent — Human Green

The primary accent is Human green (`--hu-accent`), a nature-inspired color that anchors the Human brand identity.

| Token                | Dark                  | Light                |
| -------------------- | --------------------- | -------------------- |
| `--hu-accent`        | #7AB648               | #5A9A30              |
| `--hu-accent-hover`  | #8DC63F               | #47802A              |
| `--hu-accent-strong` | #a3d46a               | #7AB648              |
| `--hu-accent-subtle` | rgba(122,182,72,0.14) | rgba(90,154,48,0.10) |

### Secondary Accent — Amber

Amber (`--hu-accent-secondary`) provides warm counterpoint for featured content, highlights, and CTAs.

| Token                          | Dark                  | Light                |
| ------------------------------ | --------------------- | -------------------- |
| `--hu-accent-secondary`        | #f59e0b               | #d97706              |
| `--hu-accent-secondary-hover`  | #fbbf24               | #b45309              |
| `--hu-accent-secondary-subtle` | rgba(245,158,11,0.14) | rgba(217,119,6,0.10) |

### Tertiary Accent — Steel Blue

Steel blue (`--hu-accent-tertiary`) adds depth for info states, data visualization, and provider-related UI (institutional steel blue, anchor `#4A6FA5` at 500).

| Token                         | Dark                  | Light                |
| ----------------------------- | --------------------- | -------------------- |
| `--hu-accent-tertiary`        | #4a6fa5               | #3d5d8c              |
| `--hu-accent-tertiary-hover`  | #7496c4               | #324a73              |
| `--hu-accent-tertiary-subtle` | rgba(74,111,165,0.14) | rgba(61,93,140,0.10) |

### Coral (Error/Destructive Only)

Coral is reserved exclusively for error and destructive states. Do not use coral as a general accent.

| Token            | Dark                   | Light                |
| ---------------- | ---------------------- | -------------------- |
| `--hu-error`     | #f97066                | #e11d48              |
| `--hu-error-dim` | rgba(249,112,102,0.12) | rgba(225,29,72,0.08) |

### Neutral Scale

Backgrounds and surfaces use a stepped neutral ladder:

| Token              | Purpose                               |
| ------------------ | ------------------------------------- |
| `--hu-bg`          | Page background                       |
| `--hu-bg-inset`    | Deepest inset (inputs, nested panels) |
| `--hu-bg-surface`  | Cards, panels, elevated content       |
| `--hu-bg-elevated` | Popovers, dropdowns                   |
| `--hu-bg-overlay`  | Modals, sheets, toasts                |

### Text Hierarchy

| Token             | Purpose                  |
| ----------------- | ------------------------ |
| `--hu-text`       | Primary body text        |
| `--hu-text-muted` | Secondary, labels, hints |
| `--hu-text-faint` | Tertiary, placeholders   |

### Semantic Colors

| Token          | Use                           |
| -------------- | ----------------------------- |
| `--hu-success` | Success states, confirmations |
| `--hu-warning` | Warnings, caution             |
| `--hu-error`   | Errors, destructive actions   |
| `--hu-info`    | Informational, links          |

---

## Typography

### Canonical Typeface: Avenir

Avenir is the canonical typeface across all platforms. Never use Google Fonts or external font CDNs.

| Platform  | Usage                                                                                |
| --------- | ------------------------------------------------------------------------------------ |
| Web       | `var(--hu-font)` — never set `font-family` directly                                  |
| iOS/macOS | `Font.custom("Avenir-Book", size:)`, `Avenir-Medium`, `Avenir-Heavy`, `Avenir-Black` |
| Android   | `AvenirFontFamily` from `Theme.kt`                                                   |
| CLI/TUI   | Terminal font; use token-derived ANSI colors from `design_tokens.h`                  |

### Type Scale Roles

| Token                   | Use                  |
| ----------------------- | -------------------- |
| `--hu-type-display-lg`  | Hero headlines       |
| `--hu-type-display-md`  | Section headings     |
| `--hu-type-headline-lg` | Card titles          |
| `--hu-type-headline-md` | Subsection titles    |
| `--hu-type-body-md`     | Primary body         |
| `--hu-type-body-sm`     | Secondary, captions  |
| `--hu-type-label-md`    | Form labels, badges  |
| `--hu-type-caption`     | Timestamps, metadata |

---

## Motion Principles

### Spring Physics

Four spring presets for different contexts:

| Token                    | Use Case                             |
| ------------------------ | ------------------------------------ |
| `--hu-spring-micro`      | Buttons, toggles, micro-interactions |
| `--hu-spring-standard`   | Panels, dropdowns                    |
| `--hu-spring-expressive` | Page transitions                     |
| `--hu-spring-dramatic`   | Hero reveals, modals                 |

### Choreography

- Use `--hu-stagger-delay` for sequential reveals
- Use `--hu-cascade-delay` for cascading lists
- Never use `setTimeout` for animation timing — use CSS transitions or `requestAnimationFrame`

### Reduced Motion

**Every animation must respect `prefers-reduced-motion: reduce`.** Use the global media query in `theme.css` or add component-level `@media (prefers-reduced-motion: reduce)` overrides. Keyframe names use `hu-` prefix (e.g., `hu-fade-in`, `hu-slide-up`).

### Theme Switch Crossfade

The dashboard uses the View Transitions API for smooth dark/light theme switching. When the user cycles themes via the sidebar toggle, the DOM mutation is wrapped in `document.startViewTransition()`, producing a crossfade:

- `::view-transition-old(root)` fades out with `--hu-duration-normal`
- `::view-transition-new(root)` fades in with the same timing
- Falls back to instant swap when View Transitions is unsupported
- Respects `prefers-reduced-motion: reduce` (0ms animation)

---

## Component API Reference

| Component        | Purpose                                                             |
| ---------------- | ------------------------------------------------------------------- |
| `hu-button`      | Primary actions; variants: primary, secondary, destructive, ghost   |
| `hu-badge`       | Status indicators; variants: success, warning, error, info, neutral |
| `hu-card`        | Content containers; glass/solid, hover/click/tilt, mesh/chromatic, entrance, tonal surfaces |
| `hu-modal`       | Centered dialog overlay; focus trap, Escape to close                |
| `hu-sheet`       | Bottom sheet overlay; swipe to dismiss                              |
| `hu-toast`       | Transient notifications                                             |
| `hu-tooltip`     | Hover/focus hints                                                   |
| `hu-progress`    | Linear progress; determinate or indeterminate                       |
| `hu-skeleton`    | Loading placeholders                                                |
| `hu-avatar`      | User avatars; initials fallback, status indicator                   |
| `hu-tabs`        | Tab navigation                                                      |
| `hu-empty-state` | Empty list/state messaging                                          |

### Component attributes

#### hu-card

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `glass` | boolean | true | Glass tier styling |
| `solid` | boolean | false | Opaque surface (no glass) |
| `hoverable` | boolean | false | Hover lift effect |
| `clickable` | boolean | false | Click feedback |
| `tilt` | boolean | false | 3D perspective tilt on pointer proximity |
| `mesh` | boolean | false | Mesh gradient background overlay |
| `chromatic` | boolean | false | Chromatic prismatic border |
| `entrance` | boolean | false | Animate into view (IntersectionObserver) with stagger support via `--hu-stagger-delay` |
| `surface` | string | "default" | Tonal surface tier: `default` (container), `high` (interactive), `highest` (emphasis) |

#### hu-input

Supports a `variant` attribute:
- Default: standard surface with elevated background
- `variant="tonal"`: uses `--hu-surface-container` background with `--hu-surface-container-high` border for branded depth

---

## Platform-Specific Notes

### CSS (Web Dashboard, Website)

- All values reference `--hu-*` custom properties from generated `_tokens.css`
- Never use raw hex colors, pixel spacing, or font-family
- Shadow: `var(--hu-shadow-sm)`, `var(--hu-shadow-md)`, `var(--hu-shadow-lg)`
- Duration: `var(--hu-duration-fast)`, `var(--hu-duration-normal)`, `var(--hu-duration-slow)`

### CSS Utility Classes (theme.css)

| Class | Purpose |
|-------|---------|
| `.hu-interactive` | Layered hover/pressed states: background shift + accent-tinted border + inset shadow on press |
| `.hu-interactive-subtle` | Lighter variant for dense UIs |
| `.hu-link-structural` | Steel blue (`--hu-accent-tertiary`) link styling for informational/structural links |
| `.hu-shadow-accent` | Accent-colored drop shadow |
| `.hu-shadow-warm` | Warm (secondary-accent) drop shadow |
| `.hu-shadow-glass` | Glass-style shadow with inset highlights |

### Swift (iOS)

- Use `HUTokens` for colors, spacing, radius
- Spring: `HUTokens.springMicro`, `springStandard`, `springExpressive`, `springDramatic`
- Font: `Font.custom("Avenir-Book", size:)` etc.

### Kotlin (Android)

- Use `DesignTokens` or generated constants from `Theme.kt`
- `AvenirFontFamily` for typography

### C (CLI/TUI)

- `include/human/design_tokens.h` provides `#define` macros for ANSI colors, spacing
- Token-derived values for consistent terminal output

---

## Token Usage Rules and Anti-Patterns

### Do

- Always use `var(--hu-*)` tokens
- Use semantic tokens (`--hu-text`, `--hu-accent`) instead of base (`--hu-bg`)
- Respect `prefers-color-scheme` and `prefers-reduced-motion`
- Use `--hu-font` / `var(--hu-font-mono)` for typography

### Don't

- Use raw hex colors (e.g., `#7AB648`) — use `var(--hu-accent)`
- Use raw pixel values for spacing — use `var(--hu-space-sm)` etc.
- Use raw `font-family` — use `var(--hu-font)`
- Use emoji as UI icons — use Phosphor Regular from `ui/src/icons.ts`
- Use Google Fonts or external font CDNs
- Create one-off SVGs when a Phosphor icon exists
