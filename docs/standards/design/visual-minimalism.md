---
title: Visual Minimalism Standard
---

# Visual Minimalism Standard

> Core design philosophy: **Add nothing. Remove everything that doesn't serve the user.**
> Every pixel must justify its existence. Chrome, decoration, and ornamentation are
> liabilities unless they directly aid comprehension or interaction.

---

## SOTA References

This standard synthesizes minimalism principles from:

- **Apple Human Interface Guidelines** — Deference: the UI defers to the content, never competing with it
- **Claude.ai** — Radical minimalism: empty states are blank canvases, chrome appears only when needed
- **Material Design 3 Expressive** — Adaptive layouts, physics-based motion, content-first surfaces
- **Dieter Rams** — "Good design is as little design as possible": less, but better

---

## 1. Core Principle: Deference

The interface exists to present the user's content. Every element that is not content is
chrome, and chrome must earn its place.

Rules:

- If removing an element does not reduce comprehension or capability, remove it
- Prefer progressive disclosure over persistent visibility
- Non-critical information appears on hover, focus, or explicit request — not by default
- The user's content (messages, data, settings) is always the visual hero

---

## 2. Benchmarks

### 2.1 Claude.ai (Radical Minimalism)

Claude.ai's empty state is a near-blank canvas: centered greeting, input field, and
suggestion chips. No sidebar, no header, no status indicators until the user acts.

Takeaway: empty states should contain only what the user needs to take their first action.

### 2.2 Apple HIG (Clarity, Deference, Depth)

Apple's three pillars map directly to Human's design:

| Pillar    | Human Application                                                       |
| --------- | ----------------------------------------------------------------------- |
| Clarity   | Legible text, purposeful icons, focused layouts                         |
| Deference | UI recedes so content advances — glass, subtle borders, muted chrome    |
| Depth     | Layered surfaces communicate hierarchy without explicit visual dividers |

### 2.3 Material 3 Expressive (Adaptive, Physics-Based)

M3 Expressive brings physics-based motion and adaptive container layouts. Human adopts:

- Spring physics for all interactive animations (see `motion-design.md`)
- Tonal surface hierarchy for implicit depth (see `design-system.md`)
- Container-responsive layouts over viewport-responsive breakpoints

---

## 3. Empty State Rules

An empty state is the user's first impression. It must feel calm, inviting, and focused.

Rules:

- Center content vertically and horizontally within the viewport
- Zero chrome: no status bars, no persistent headers, no banners, no navigation unless essential
- Compose the empty state as a single centered unit: greeting + input + suggestions
- Greeting text uses a display or serif font for warmth and personality
- Composer border is nearly invisible (`--hu-border-subtle` at reduced opacity or transparent)
- Suggestion chips sit below or adjacent to the composer — never competing with primary content
- Background is clean: `--hu-bg` with no patterns, gradients, or decorative elements

```
╔═══════════════════════════════════════╗
║                                       ║
║                                       ║
║                                       ║
║           Good morning, Seth.         ║  ← Display/serif, centered
║                                       ║
║      ┌─────────────────────────┐      ║  ← Composer, nearly invisible border
║      │                         │      ║
║      └─────────────────────────┘      ║
║        [chip]  [chip]  [chip]         ║  ← Suggestion chips, muted
║                                       ║
║                                       ║
║                                       ║
╚═══════════════════════════════════════╝
```

---

## 4. Glass and Surface Rules

Glass effects add depth and sophistication but become noise when overused.

Rules:

- Maximum background opacity for glass surfaces: 35% (`color-mix(in srgb, var(--hu-bg-surface) 35%, transparent)`)
- Maximum blur for subtle-tier glass: `8px` (`backdrop-filter: blur(8px)`)
- Glass should reveal content beneath it — if the glass is opaque enough to fully obscure, use a solid surface instead
- Reserve heavier blur tiers (16px+) for modals and overlays where obscuring background is intentional
- Never stack multiple glass layers — at most one glass surface between the user and the background
- Composer borders on empty state: use `--hu-border-subtle` or transparent — the border should be felt, not seen

---

## 5. Banner Rules

Banners communicate transient status. They must never disrupt the content flow.

Rules:

- Position: `position: fixed` — banners float over content, never push it down
- Auto-dismiss: non-critical banners dismiss after 5 seconds
- Dismissable: every banner has a close action
- Memory: dismissed banners record state in `sessionStorage` — never re-show within a session
- Entrance: fade in with `--hu-duration-fast` — no slide or bounce
- Exit: fade out with `--hu-duration-fast`
- Placement: top-center of viewport, below any persistent navigation
- Maximum one banner visible at a time — queue additional banners
- Never use banners for permanent status — use inline indicators instead

---

## 6. Scrollbar Rules

Scrollbars are infrastructure, not UI. They should be invisible until needed.

Rules:

- Width: `6px` (thin profile)
- Track: transparent
- Thumb color: `--hu-border-subtle` — neutral, not branded
- Auto-hiding: thumb fades out after scroll inactivity (CSS `scrollbar-gutter: stable`)
- Hover: thumb may increase to `8px` and brighten slightly on pointer proximity
- Corner: no visible scrollbar corner (use `scrollbar-width: thin` or `::-webkit-scrollbar` overrides)
- Never style scrollbars with accent colors — they are infrastructure, not interactive elements

---

## 7. Floating Element Rules

Floating elements (tooltips, popovers, toasts, FABs) compete with content for attention.

Rules:

- Scope floating elements to the relevant view only — a chat-view tooltip should not appear over settings
- Floating elements must never obscure the primary content area's focal point
- Use fade transitions (`--hu-duration-fast`, `opacity` only) for appear/disappear — no slide or scale
- Maximum one persistent floating element per viewport (toasts queue, tooltips replace)
- Floating action buttons (FABs): avoid unless the action is genuinely the most common user need
- Z-index: use the established layer hierarchy (`--hu-z-dropdown`, `--hu-z-modal`, `--hu-z-toast`)

---

## 8. Typography Rules

### 8.1 Greeting and Display Type

The greeting is the emotional entry point. It uses a serif or display font for warmth:

- Greeting text: serif typeface (e.g., system serif stack or a loaded display font)
- Body and UI text: sans-serif (`var(--hu-font)` — Avenir)
- Never mix more than two font families on a single screen
- Display/serif is reserved for greetings and hero moments — not for body copy or UI labels

### 8.2 Typographic Restraint

- Maximum three type sizes visible in any single view section (see `visual-standards.md` §7.1)
- Prefer weight and color variation over size variation for hierarchy within a section
- Muted text (`--hu-text-muted`) is the default for secondary information — not smaller font size

---

## 9. Status Bar and Chrome Visibility

Status information (connection state, model name, token count) is useful but rarely urgent.

Rules:

- Status bars are hidden by default unless the information is actionable
- Non-critical status appears on hover or focus — reveal on pointer proximity
- Critical status (errors, disconnection) surfaces immediately and persistently
- Status indicators use semantic color dots (`--hu-success`, `--hu-error`, `--hu-warning`) — compact, not verbose
- Never dedicate a full-width bar to status — use inline indicators within existing UI regions

---

## 10. The Squint Test

> If you squint at the screen, only the content should be visible — not the chrome, not the
> borders, not the controls.

This is the final validation for every UI change. When the screen is viewed at a distance or
with blurred vision:

1. The user's content (messages, data, forms) should be the dominant visual element
2. Navigation, toolbars, and status bars should recede into the background
3. Borders and dividers should be imperceptible
4. Only the primary action (CTA, send button) should stand out beyond the content

If chrome is visible when you squint, the design has too much of it.

---

## 11. Quality Checklist

Before shipping any UI change, verify:

- [ ] **Squint test**: Does only the content remain visible when you squint?
- [ ] **Empty state**: Is it calm, centered, and chrome-free?
- [ ] **Greeting font**: Does the greeting use serif/display for warmth?
- [ ] **Composer border**: Nearly invisible on empty state?
- [ ] **Banners**: Float over content (never push)? Auto-dismiss? Session memory?
- [ ] **Scrollbars**: Thin (6px)? Neutral color? Auto-hiding?
- [ ] **Glass**: Max 35% opacity? Max 8px blur (subtle tier)?
- [ ] **Floating elements**: Scoped to relevant view? Fade transitions?
- [ ] **Status**: Hidden unless actionable or critical?
- [ ] **Font families**: Maximum two on screen (serif greeting + Avenir body)?
- [ ] **Progressive disclosure**: Is non-critical info revealed on interaction, not shown by default?
- [ ] **One hero**: Is there exactly one focal point per viewport?

---

## 12. Cross-Reference

| Document             | Related Coverage                          |
| -------------------- | ----------------------------------------- |
| `visual-standards.md`| Visual hierarchy, emphasis levels, 60-30-10 rule |
| `design-system.md`   | Token values, component API, platform notes |
| `motion-design.md`   | Spring physics, choreography, reduced motion |
| `ux-patterns.md`     | Layout archetypes, progressive disclosure |
| `spacing-discipline.md` | Spacing scale, content width, layout patterns |
