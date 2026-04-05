---
title: Spacing Discipline Standard
---

# Spacing Discipline Standard

> Every unit of space is intentional. Spacing tokens are a shared vocabulary — they communicate
> relationships between elements (proximity), establish rhythm, and prevent visual noise.
> Raw pixel values are banned. Token-based spacing is the only spacing.

---

## 1. Spacing Scale

Human's spacing scale is derived from a 4px base unit. Each token has a semantic purpose —
choosing the right token is not about matching a pixel value but about expressing the
relationship between elements.

| Token              | Value          | Pixel Equiv | Semantic Purpose                                      |
| ------------------ | -------------- | ----------- | ----------------------------------------------------- |
| `--hu-space-2xs`   | `0.125rem`     | 2px         | Micro-adjustments: icon optical alignment, hairline gaps |
| `--hu-space-xs`    | `0.25rem`      | 4px         | Tight internal padding, inline element gaps, icon-to-label spacing |
| `--hu-space-sm`    | `0.5rem`       | 8px         | Standard internal padding, gaps between related items  |
| `--hu-space-md`    | `1rem`         | 16px        | Section padding, gaps between distinct (unrelated) elements |
| `--hu-space-lg`    | `1.5rem`       | 24px        | View-level padding, major section breaks               |
| `--hu-space-xl`    | `2rem`         | 32px        | Hero sections, major breathing room                    |
| `--hu-space-2xl`   | `3rem`         | 48px        | Page-level padding, empty state centering margins      |
| `--hu-space-3xl`   | `4rem`         | 64px        | Maximum breathing room, scroll terminus spacing        |
| `--hu-space-4xl`   | `5rem`         | 80px        | Reserved for extraordinary layout needs                |
| `--hu-space-5xl`   | `6rem`         | 96px        | Reserved for extraordinary layout needs                |

### 1.1 When to Use Each Token

**`2xs` (2px)** — Surgical precision. Use when two elements need to be visually adjacent but
not touching. Examples: aligning an icon baseline with text, hairline gap between stacked
borders, optical correction for asymmetric glyphs.

**`xs` (4px)** — Tight coupling. Elements are part of the same atomic unit. Examples:
icon-to-label gap in a button, gap between inline badges, internal padding of a tag/chip.

**`sm` (8px)** — Related siblings. Elements belong to the same group but are individually
distinct. Examples: padding inside a card, gap between list items in a compact list, gap
between a label and its input.

**`md` (16px)** — Distinct elements. Elements are peers but not part of the same group.
Examples: gap between form fields, padding between card sections, gap between sidebar items.

**`lg` (24px)** — Section-level. Marks a clear boundary between logical sections of a view.
Examples: padding around a view's content area, gap between a toolbar and its content, major
section breaks within a settings page.

**`xl` (32px)** — Emphasis spacing. Creates deliberate visual breathing room. Examples: hero
section padding, gap above a page title, spacing between major dashboard regions.

**`2xl` (48px)** — Page-level. The outermost spacing in a layout or the centering margin for
empty states. Examples: top/bottom padding for a centered empty state, spacing between footer
and content.

**`3xl` (64px)** — Terminal spacing. The maximum gap in standard layouts. Examples: scroll
terminus (space below the last element before scroll ends), dramatic section dividers.

---

## 2. Content Width Tokens

Horizontal content width is constrained to maintain readable line lengths and visual focus.

| Token                      | Value    | Use                                              |
| -------------------------- | -------- | ------------------------------------------------ |
| `--hu-content-width`       | `48rem`  | Standard views: chat, voice, overview            |
| `--hu-content-width-wide`  | `72rem`  | Admin/dense views: settings, agents, memory      |

Rules:

- Every scrollable content area uses `max-width: var(--hu-content-width)` with `margin: 0 auto`
- Wide views (`settings`, `agents`, `memory`) use `--hu-content-width-wide`
- Never allow content to stretch to the full viewport width — unconstrained text becomes unreadable
- Sidebars, navigation, and toolbars are exempt from content width constraints
- Content width tokens are the source of truth — never hardcode a `max-width` value

---

## 3. Component Spacing Contract

Components are spacing-agnostic by default. The parent controls external spacing.

### 3.1 The Rule

> Components have zero external margin. The parent container controls spacing via `gap` on
> flex or grid layouts.

This means:

- A `<hu-card>` has internal padding but zero margin
- A `<hu-button>` has internal padding but zero margin
- A `<hu-badge>` has internal padding but zero margin
- The parent (a view, a section, a flex container) sets `gap: var(--hu-space-md)` to space its children

### 3.2 Why

Margin on reusable components creates coupling — the component assumes knowledge of its
context. In one layout it might need `16px` of bottom margin; in another, `8px`. By keeping
components margin-free:

- The same component works in any layout without overrides
- Spacing is controlled from a single place (the container)
- Gap-based spacing automatically handles first/last child (no `:last-child { margin-bottom: 0 }`)

### 3.3 Internal Padding

Components own their internal spacing:

| Component Type | Internal Padding Token | Rationale                              |
| -------------- | ---------------------- | -------------------------------------- |
| Cards          | `--hu-space-lg`        | Generous breathing room for content    |
| Buttons        | `--hu-space-sm` / `--hu-space-lg` | Vertical tight, horizontal generous |
| Inputs         | `--hu-space-sm`        | Compact but comfortable touch target   |
| Badges/Tags    | `--hu-space-xs`        | Tight, inline-level elements           |
| List items     | `--hu-space-sm` / `--hu-space-md` | Depends on density context    |

---

## 4. Anti-Patterns

### 4.1 Double Spacing

Never combine `margin` and `gap` on the same axis. Choose one source of truth.

```css
/* BAD — double spacing */
.container {
  display: flex;
  flex-direction: column;
  gap: var(--hu-space-md);
}
.container > .item {
  margin-bottom: var(--hu-space-sm);  /* conflicts with gap */
}

/* GOOD — single source of truth */
.container {
  display: flex;
  flex-direction: column;
  gap: var(--hu-space-md);
}
```

### 4.2 Raw Pixel Values

Never use raw pixel values for spacing. Always use tokens.

```css
/* BAD */
padding: 13px;
margin-top: 20px;
gap: 6px;

/* GOOD */
padding: var(--hu-space-md);
margin-top: var(--hu-space-lg);
gap: var(--hu-space-xs);
```

### 4.3 Magic Numbers

If a spacing value doesn't map to a token, the design needs adjustment — not a one-off value.

```css
/* BAD — what is 13px? */
padding: 13px 17px;

/* GOOD — use the nearest tokens */
padding: var(--hu-space-md) var(--hu-space-lg);
```

### 4.4 Viewport-Relative Spacing

Avoid `vh`, `vw`, `vmin`, `vmax` for component spacing. Viewport units scale unpredictably
across devices and create inconsistent rhythm.

```css
/* BAD */
padding-top: 10vh;

/* GOOD */
padding-top: var(--hu-space-2xl);
```

Exception: full-height layouts (`min-height: 100dvh`) and centering containers are acceptable
uses of viewport units for layout structure — not for spacing between elements.

### 4.5 Skipping Scale Steps

Don't jump more than two steps in the scale within the same visual context. Extreme jumps
create visual dissonance.

```css
/* BAD — jumping from xs to 2xl in the same card */
.card {
  padding: var(--hu-space-xs);
  margin-bottom: var(--hu-space-2xl);
}

/* GOOD — proportional spacing */
.card {
  padding: var(--hu-space-lg);
  /* parent controls external spacing */
}
```

---

## 5. View Layout Pattern

Every view follows a consistent layout pattern:

```css
.view-content {
  max-width: var(--hu-content-width);
  margin: 0 auto;
  padding: var(--hu-space-lg);
  display: flex;
  flex-direction: column;
  gap: var(--hu-space-md);
}
```

This pattern ensures:

- Content is centered and constrained to a readable width
- Horizontal padding provides breathing room from viewport edges
- Vertical rhythm between sections is consistent via `gap`
- No margin on child components — `gap` handles all inter-element spacing

For wide/admin views:

```css
.view-content-wide {
  max-width: var(--hu-content-width-wide);
  margin: 0 auto;
  padding: var(--hu-space-lg);
}
```

---

## 6. Responsive Scale

At mobile breakpoints, spacing may reduce by one step to preserve content density:

| Desktop Token      | Mobile Reduction    | Rationale                          |
| ------------------ | ------------------- | ---------------------------------- |
| `--hu-space-lg`    | `--hu-space-md`     | View padding: less room to spare   |
| `--hu-space-md`    | `--hu-space-sm`     | Section gaps: tighter on small screens |
| `--hu-space-xl`    | `--hu-space-lg`     | Hero spacing: proportional reduction |

Rules:

- Never reduce by more than one step at a single breakpoint
- Internal component padding (buttons, inputs, badges) does not change — touch targets must remain consistent
- Use container queries (`@container`) where possible instead of viewport media queries
- Breakpoint tokens should be used for responsive thresholds — not raw pixel values

---

## 7. Spacing Rhythm

Consistent spacing creates visual rhythm, which reduces cognitive load and communicates
structure without explicit dividers.

### 7.1 Vertical Rhythm

| Context                   | Token              | Example                           |
| ------------------------- | ------------------ | --------------------------------- |
| Within a component        | `sm` to `md`       | Label to input, icon to text      |
| Between sibling elements  | `md` to `lg`       | Card to card, field to field      |
| Between sections          | `lg` to `xl`       | Header to content, content to footer |
| Between major regions     | `xl` to `2xl`      | Dashboard sections, page regions  |

### 7.2 Horizontal Rhythm

| Context                   | Token              | Example                           |
| ------------------------- | ------------------ | --------------------------------- |
| Inline elements           | `xs` to `sm`       | Icon + label, badge + text        |
| Sidebar to content        | `lg`               | Navigation boundary               |
| Content to viewport edge  | `lg` to `xl`       | View padding                      |

---

## 8. Quality Checklist

Before shipping any spacing change, verify:

- [ ] **No raw pixels**: Every spacing value uses a `--hu-space-*` token?
- [ ] **No double spacing**: No element has both `margin` and `gap` on the same axis?
- [ ] **No magic numbers**: Every spacing value maps to a token in the scale?
- [ ] **Component contract**: Components have zero external margin?
- [ ] **Content width**: Scrollable areas use `--hu-content-width` or `--hu-content-width-wide`?
- [ ] **Responsive**: Mobile spacing reduces by at most one step?
- [ ] **Rhythm**: Spacing feels consistent and proportional across the view?
- [ ] **Scale steps**: No jumps greater than two steps within the same visual context?
- [ ] **Gap over margin**: Flex/grid containers use `gap` instead of child margins?

---

## 9. Cross-Reference

| Document               | Related Coverage                                    |
| ---------------------- | --------------------------------------------------- |
| `visual-standards.md`  | Spacing system (§3), whitespace as design (§3.3)    |
| `visual-minimalism.md` | Empty state centering, breathing room philosophy    |
| `design-system.md`     | Token values, platform-specific usage               |
| `ux-patterns.md`       | Layout archetypes, responsive patterns              |
