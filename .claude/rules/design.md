---
paths: ui/**/*.ts, ui/**/*.css, website/**/*.astro, design-tokens/**/*
---

# Design Rules (Mandatory)

Read `docs/standards/design/visual-minimalism.md` and `docs/standards/design/spacing-discipline.md` before any UI change.

## Visual Minimalism

- Empty states: center content vertically, zero chrome
- Status bars: hidden on empty state, subtle (opacity 0.7) with messages
- Glass effects: max 35% background opacity, max 8px blur for subtle tier
- Banners: position fixed, auto-dismiss, sessionStorage memory
- Scrollbars: thin 6px, neutral `--hu-border-subtle`, auto-hiding
- Floating elements: scope to relevant view only
- Greeting: serif display font for warmth
- Composer: near-invisible border on empty state

## Spacing Discipline

- NEVER use raw pixel values -- use `--hu-space-*` tokens
- Components are margin-free -- parent controls spacing via gap
- Content width: `var(--hu-content-width)` for views, `var(--hu-content-width-wide)` for admin
- Never combine margin + gap on the same axis (double-spacing anti-pattern)
- Viewport-relative spacing (vh/vw) prohibited for padding -- use fixed tokens

## Before Any UI Commit

- Run `npm run lint:tokens` -- 0 violations
- Verify `prefers-reduced-motion: reduce` on all new animations
- All colors use `--hu-*` tokens or `color-mix()`
- Squint test: only content visible, not chrome

## Standards

- Read `docs/standards/design/visual-minimalism.md` for minimalism rules
- Read `docs/standards/design/spacing-discipline.md` for spacing semantics
- Read `docs/standards/design/visual-standards.md` for hierarchy and composition
- Read `docs/standards/design/motion-design.md` for animation rules
