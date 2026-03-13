---
title: Human Quality Scorecard
updated: 2026-03-09
---

# Human Quality Scorecard

> Per-surface quality scores across 7 dimensions. Updated quarterly.
> Target: score higher than every benchmark brand on every dimension.

## Scoring Scale

| Score | Meaning                                  |
| ----- | ---------------------------------------- |
| 1-3   | Below industry average                   |
| 4-5   | Industry average                         |
| 6-7   | Above average (most SaaS products)       |
| 8     | Best-in-class (Linear, Stripe tier)      |
| 9     | Category-defining (Awwwards winner tier) |
| 10    | Sets the standard others measure against |

## Current Scores (Q1 2026 — Post-Sprint Re-Audit)

| Surface   | Perf | Visual | Motion | Density | A11y | Brand | Innovation | Total | Target      |
| --------- | ---- | ------ | ------ | ------- | ---- | ----- | ---------- | ----- | ----------- |
| Website   | 8    | 8      | 7      | 7       | 9    | 8     | 7          | 54/70 | 63+ (9 avg) |
| Dashboard | 8    | 8      | 8      | 7       | 9    | 8     | 8          | 56/70 | 63+ (9 avg) |
| iOS       | 5    | 6      | 6      | 6       | 6    | 7     | 5          | 41/70 | 63+ (9 avg) |
| macOS     | 5    | 6      | 6      | 6       | 6    | 7     | 5          | 41/70 | 63+ (9 avg) |
| Android   | 4    | 5      | 5      | 5       | 5    | 6     | 5          | 35/70 | 63+ (9 avg) |

## Benchmark Comparison

| Brand        | Perf | Visual | Motion | Density | A11y | Brand | Innovation | Total |
| ------------ | ---- | ------ | ------ | ------- | ---- | ----- | ---------- | ----- |
| Linear       | 4    | 9      | 9      | 9       | 6    | 9     | 8          | 54/70 |
| Vercel       | 6    | 8      | 7      | 8       | 7    | 8     | 8          | 52/70 |
| Stripe       | 6    | 9      | 8      | 8       | 9    | 9     | 7          | 56/70 |
| Superhuman   | 6    | 8      | 8      | 8       | 9    | 8     | 7          | 54/70 |
| Raycast      | 7    | 9      | 9      | 9       | 7    | 9     | 8          | 58/70 |
| Figma        | 4    | 9      | 7      | 8       | 7    | 9     | 9          | 53/70 |
| Lando Norris | 5    | 10     | 10     | 6       | 5    | 8     | 10         | 54/70 |

## Dimension Rubrics

Detailed rubrics for each of the 7 dimensions, with concrete criteria for achieving a score of 9.

**Performance (automated)**:

- 1-5: Lighthouse <90, LCP >2s, CLS >0.1
- 6-7: Lighthouse 90-95, LCP 1-2s, CLS <0.1
- 8: Lighthouse 95-97, LCP <1s, CLS <0.05
- 9: Lighthouse 98-99, LCP <0.5s, CLS ~0
- 10: Lighthouse 100, LCP <0.3s, CLS 0.00, INP <50ms

**Criteria for 9**: Lighthouse 98+ on all pages; LCP <0.5s (critical CSS inlined, fonts preloaded, hero images priority-fetched); CLS 0.00 (skeleton dimensions match real content); initial bundle <150KB; all lazy chunks <100KB; `requestIdleCallback` for deferred work.

**Visual Craft (manual audit)**:

- 1-5: Inconsistent spacing, raw hex colors, mixed type scales
- 6-7: Token-based, consistent but unremarkable
- 8: Stripe/Linear tier — mathematically precise, dark/light coherent
- 9: Awwwards nomination quality — every pixel intentional
- 10: Sets new standard — competitors study your UI for inspiration

**Criteria for 9**: Zero raw hex/px in any component (lint enforced); dark+light pixel-perfect on every view; editorial typography hierarchy with full scale usage (hero/display/heading/body/caption); tonal surfaces for all branded elevation; squint-test passes on every view (primary action and content area identifiable at a glance); 60-30-10 color ratio validated; accent used on <10% of UI surface (restraint like Raycast).

**Motion Quality (manual audit)**:

- 1-5: CSS transitions only, no spring physics, jarring
- 6-7: Smooth transitions, basic easing tokens
- 8: Spring-first, stagger choreography, reduced-motion support
- 9: Physics-accurate springs, scroll-driven narratives, ambient intelligence
- 10: Indistinguishable from native platform animations on every surface

**Criteria for 9**: Spring physics on all interactive elements (buttons, toggles, cards, tabs); scroll-driven entrance animations on every card/list using `animation-timeline: scroll()` with IntersectionObserver fallback; View Transitions API on all route changes with named shared elements; choreographed stagger on all lists (50ms delay, 300ms cap); Disney staging on all modals/sheets (backdrop dim + spring scale); every animation respects `prefers-reduced-motion`.

**Information Density (heuristic evaluation)**:

- 1-5: Cluttered or wastefully sparse, legends required
- 6-7: Clean layout, standard data presentation
- 8: Tufte-level data-ink ratio, progressive disclosure
- 9: Information Theater — understanding is involuntary
- 10: Zero learning curve for any data visualization

**Criteria for 9**: Direct labels preferred over legends on all charts; sparklines and inline metrics for at-a-glance data; progressive disclosure on all complex forms/settings; animated numbers (`hu-animated-number`) for all live values; tabular figures for numeric columns; small multiples for comparative data.

**Accessibility (automated + manual)**:

- 1-5: WCAG violations, missing focus management
- 6-7: WCAG AA compliant, basic keyboard support
- 8: 98+ Lighthouse a11y, full keyboard nav, screen reader tested
- 9: 100 Lighthouse, VoiceOver/TalkBack tested, high contrast mode
- 10: Exceeds WCAG AAA, cognitive accessibility considered

**Criteria for 9**: Lighthouse Accessibility 100 on all pages; axe-core zero violations in E2E; full keyboard navigation (Tab, Escape, Enter/Space, Arrow keys); animated focus ring with spring expansion; `prefers-contrast: more` fully supported; `prefers-reduced-motion` fully supported; all modals trap focus and support Escape; all tooltips accessible via keyboard.

**Brand Cohesion (cross-platform audit)**:

- 1-5: Inconsistent across surfaces, no design system
- 6-7: Shared tokens, mostly consistent
- 8: Unified system across web + native, platform-appropriate adaptations
- 9: Each surface feels native AND unmistakably Human
- 10: Platform Transcendence — best-designed app on every device

**Criteria for 9**: Dashboard and website import from same `design-tokens/` source; identical button/card/input styling between dashboard and website; same spring easing and stagger patterns on both surfaces; Avenir + Phosphor consistent everywhere; 60-30-10 color ratio matches across surfaces; cross-surface visual audit passes (automated screenshot comparison).

**Innovation (feature inventory)**:

- 1-5: Standard patterns only
- 6-7: Modern CSS, reasonable interactivity
- 8: Cutting-edge CSS (container queries, view transitions)
- 9: Features competitors don't have yet (spatial UI, ambient intelligence)
- 10: Defining new interaction paradigms

**Criteria for 9**: At least 2 platform-leading CSS features adopted (scroll-driven animations via `animation-timeline`, `@starting-style` for enter animations, native `popover` attribute, Anchor Positioning API); at least 1 interaction pattern not seen in benchmark brands; optimistic UI on primary actions (chat send, config save); command palette with glass backdrop and spring-animated items.

## History

| Quarter            | Website | Dashboard | iOS | macOS | Android |
| ------------------ | ------- | --------- | --- | ----- | ------- |
| Q1 2026 (baseline) | 52      | 52        | 39  | 39    | 34      |
| Q1 2026 (sprint 1) | 54      | 56        | 39  | 39    | 34      |
| Q1 2026 (sprint 2) | 54      | 56        | 41  | 41    | 35      |

Sprint 2 changes: iOS/macOS spring animation standardization, design token tint colors, Avenir fonts in SettingsView. Android design token alignment. Dashboard reduced-motion on all views, CI quality checks now enforcing.

## Action Items from Last Review

- [x] **Website Performance**: Lighthouse 96 → 97+ targeted. LCP optimizations applied: font priority tuning, hero image `loading="eager"`, Astro inline stylesheets, design-tokens prebuild wired into website.
- [x] **Visual Craft (all surfaces)**: 7→8. Scroll-driven animations, glass system on tooltips, spring easing on modal/dialog/command-palette/toast/sidebar/tabs.
- [x] **Motion Quality (dashboard)**: 8. Spring expansion complete. View Transitions with named shared elements. Scroll entrance auto-applied. All views respect `prefers-reduced-motion`.
- [~] **Motion Quality (native apps)**: 5→6. iOS springs standardized to `HUTokens.springExpressive`, hardcoded `.spring()` calls replaced. Next: Android Compose spring animations.
- [~] **Native App Maturity**: iOS/macOS 39→41, Android 34→35. iOS now uses token-based tint, Avenir fonts in settings, spring animations throughout. Next: new views (overview, sessions), Android Compose UI.
- [x] **Brand Cohesion**: 7→8. Website/dashboard token alignment, font unification, CI enforcing component quality and unused token audits.
- [x] **Accessibility**: 8→9 (dashboard). `prefers-reduced-motion` on all 8 views, Lighthouse CI thresholds at 99%.
- [ ] **Performance (dashboard)**: Target Lighthouse 99+. Need to measure and optimize.
- [ ] **Innovation**: Explore spatial UI patterns, ambient intelligence indicators.

## Related

- [Competitive Benchmarks](competitive-benchmarks.md)
- [Design doc](plans/2026-03-09-competitive-intelligence-design.md)
