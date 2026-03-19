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

## Current Scores (Q1 2026 — Sprint 12, post-SOTA)

| Surface   | Perf | Visual | Motion | Density | A11y | Brand | Innovation | Total | Target      |
| --------- | ---- | ------ | ------ | ------- | ---- | ----- | ---------- | ----- | ----------- |
| Website   | 10   | 9      | 9      | 9       | 10   | 9     | 10         | 66/70 | 63+ (9 avg) |
| Dashboard | 9    | 9      | 9      | 9       | 9    | 9     | 10         | 64/70 | 63+ (9 avg) |
| iOS       | 9    | 9      | 9      | 9       | 9    | 9     | 9          | 63/70 | 63+ (9 avg) |
| macOS     | 9    | 9      | 9      | 9       | 9    | 9     | 9          | 63/70 | 63+ (9 avg) |
| Android   | 9    | 9      | 9      | 9       | 9    | 9     | 9          | 63/70 | 63+ (9 avg) |

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
| Q1 2026 (sprint 3) | 54      | 56        | 41  | 44    | 41      |
| Q1 2026 (sprint 4) | 57      | 56        | 43  | 44    | 42      |
| Q1 2026 (sprint 5) | 57      | 57        | 48  | 48    | 44      |
| Q1 2026 (sprint 7) | 59      | 58        | 51  | 50    | 49      |
| Q1 2026 (sprint 8) | 63      | 61        | 56  | 55    | 55      |
| Q1 2026 (sprint 9) | 63      | 62        | 58  | 57    | 57      |
| Q1 2026 (sprint 12) | 66     | 64        | 63  | 63    | 63      |

Sprint 3 changes: macOS app fully tokenized (HUTokens, Avenir, spring animations, tonal surfaces). Android Compose UI built from scratch (Overview, Chat, Settings screens with Material 3 + HUTokens, spring animations). Website hardcoded hex replaced with tokens. Lighthouse thresholds tightened (98+ performance, 100 a11y). Bundle budget reduced to 300KB. Automated quality score reporting in CI. Award-winning quality criteria document created. Component quality check bug fixed.

Sprint 4 changes: Website — Three.js WebGL hero particle field, CSS scroll-driven animations (scroll-timeline, view-timeline), @view-transition CSS for cross-document transitions, @starting-style enter animations, custom cursor with hover states, branded scrollbar, micro-gradients on interactive elements, typography-as-hero treatment, comparison section redesigned, prefers-contrast:more + forced-colors support, Three.js bundled, HTML compression, GitHub Pages deployment with custom domain. Dashboard — ambient loading indicator in sidebar header. iOS — App Intents for Siri (send message + check status), Live Activity for active conversations. Android — GatewayClient WebSocket (OkHttp), Glance home screen widget. CI — quality gate 80%, LCP 1000ms, competitive benchmark on push.

Sprint 5 changes: macOS — WindowGroup with NavigationSplitView for multi-window support, sidebar navigation across 5 tabs, full keyboard shortcuts (Cmd+1-4 for tab navigation, Cmd+R for service control, Cmd+Shift+O for browser), toolbar service status indicator. iOS — Dynamic Type confirmed (relativeTo: on all custom fonts), pull-to-refresh on Overview, searchable Sessions list, additional VoiceOver accessibility labels on Settings buttons and session detail. Android — TalkBack content descriptions added to SessionsScreen (session items, delete actions), ToolsScreen (tool cards, heading), SettingsScreen (gateway URL, connection status, connect button). Dashboard — Lighthouse CI config added for localhost:3000 with accessibility and CLS thresholds. Quality — award criteria updated (keyboard/trackpad macOS, Dynamic Type iOS, Accessibility Scanner Android checked off).

Sprint 8 changes: **C Runtime** — Fixed 8+ compile errors (mcp.c, forgetting.c, corrective_rag.c, eval.c, embedded.c, computer_use.c, lsp.c, realtime.c, webrtc.c, context.c). 5,048 tests pass, 0 ASan errors. ML val_embed test stabilized. **Website** — Lighthouse **100**/100/96. LCP 0.8s via font-display:swap and deferred scripts. **Dashboard** — Lighthouse **98**/98/96. Shiki WASM eliminated (622KB), 150+ lang chunks→22, build 1.5s→0.75s. Visual Craft 8→9: 60-30-10 color ratio enforced, tonal surface elevation on session cards, tabular-nums, sparklines on session stats, typography hierarchy audit, config view spacing. **iOS** — Performance: LazyView wrappers on all tab destinations, deferred gateway via `connectIfNeeded()`. Visual: typography hierarchy (text2xl titles, textXl headings, textBase body, textSm captions), tonal `surfaceContainerHigh` on cards, accent restraint (muted stat icons). Motion: `springInteractive` (0.35/0.86) on all interactive elements, staggered list entrance on Overview and Tools. **macOS** — Performance: LazyView on all detail panes, deferred gateway. Visual: same typography/surface/accent treatment. Motion: staggered entrance on Overview stats, Sessions, Tools. **Android** — Performance: deferred gateway `connectIfNeeded()`, `@Immutable` on data classes, baseline profiles. Visual: Material 3 typography (30sp/22sp/16sp), tonal surface elevation, accent restraint. Motion: spring physics replacing tween, staggered session list entrance, predictive back handler. Brand: Phosphor SVG vector drawables replacing Material Icons in nav bar. **Flutter** — Performance: lazy screen loading (switch vs IndexedStack), deferred gateway, const constructors, RepaintBoundary. Visual: Material 3 typography hierarchy, tonal surface tokens. Density: overview shows model/latency/memory, sessions show preview+timestamp+count, settings shows ping latency. **Cross-surface** — audit-cross-surface.sh passes. Raw hex and duration violations fixed. 5 new motion tokens. **Deployment** — vercel.json, wrangler.toml, \_headers, .env.production.

Sprint 9 changes: **Native Density 8→9** — iOS: `.contentTransition(.numericText())` on stat values, `DisclosureGroup` for Advanced settings, connection latency display, message count badges + preview text on sessions. macOS: `SparklineView` (7-point Path) next to stat cards, uptime/model/memory metadata, message count + preview + relative timestamps on sessions, usage counts on tools, `DisclosureGroup` Advanced settings. Android: `AnimatedContent` with `slideInVertically` for stat transitions, message count badges + preview text, `AnimatedVisibility` collapsible Advanced section with latency. Flutter: `AnimatedSwitcher` on stat values, `ExpansionTile` Advanced section, message count badges, truncated previews. **Native Accessibility 8→9** — iOS: `.accessibilityHidden(true)` on decorative icons, `.accessibilitySortPriority(1)` on stats grid, `.accessibilityAction(.escape)` on modals. macOS: `.focusSection()` on sidebar and detail panes, `.accessibilityElement(children: .contain)` on card groups. Android: `Modifier.semantics { heading() }` on screen titles, `stateDescription` on connection indicator, `clearAndSetSemantics` on decorative dots. Flutter: `Semantics(header: true)` on titles, `ExcludeSemantics` on decorative elements, `Semantics(label:)` on icon-only buttons. **Dashboard Visual** — Sidebar: active item gets 3px left accent border, `surfaceContainerHigh` background, spring transitions. Glass: dark mode card bg 75% mix, light mode 65%. Container queries on 7 views. CSS nesting on composer and automation card. **Deploy CI** — Vercel workflow for website, Cloudflare Pages workflow for dashboard, both with design-token build step.

Sprint 7 changes: **Website** — SOTA CSS features: `:has()` selector for contextual card/nav styling, `field-sizing: content` for auto-growing inputs, Anchor Positioning API for tooltip placement with `@position-try` fallback, `text-wrap: balance` on headings. **Dashboard** — Accessibility pass on 12 components (aria-label, role attributes for sparkline, stat-card, timeline, empty-state, latex, message-stream, metric-row, overview-stats, skill-detail, status-dot, stats-row, form-group); native `<dialog>` cancel event fix (eager dispatch); dependency updates (vitest 4.1, dompurify, katex, shiki, happy-dom). **iOS** — `matchedGeometryEffect` hero transitions on Sessions (list→detail) and Tools (card→expanded overlay) with spring physics (response: 0.35, dampingFraction: 0.86). **macOS** — Full Chat view (message list, TextEditor input, Cmd+Return to send, gateway integration), Sessions view (searchable list, split detail, Cmd+Delete), Tools view (LazyVGrid, toggle cards, hover effects). **Android** — GatewayClient `request()` method for JSON-RPC; Sessions and Tools screens wired to live gateway data with refresh buttons; TalkBack accessibility maintained. **Infra** — npm audit fix (flatted vuln), Cloudflare deploy workflow fixed (design-tokens install/build step added), CMake parallel build race fixed (pre-create object dirs), version string unified to 0.4.0 across all 5 sources (main.c, version.c, mcp_server.c, status.c, cli_commands.c, main_wasi.c), `rope_theta` added to GPT config struct.

Sprint 12 changes (SOTA UX push): **Foundation** — Anticipatory UX engine (idle/hover/adjacent prefetch on dashboard, iOS, Android). Ambient intelligence (status breathing, glass-by-scroll via `animation-timeline: scroll()`, time-aware evening warmth). Interaction latency contract (INP/TTI Lighthouse assertions, Long Animation Frame E2E). **Dashboard** — Complete state matrix: chat view skeleton + branded empty state + suggestion chips + container queries + entrance animations. Responsive container queries on automations/channels/metrics/nodes. Scroll entrance on config/metrics/models/voice. Voice empty state. Metric count-up with ease-out cubic. Keyboard-first: vim j/k, / search, g+key two-key combos, shortcut hints on hover (1s delay). Command palette expanded with theme/export. Virtual scroll logs with IntersectionObserver windowing, search highlighting, .txt export, level color coding. 4-breakpoint responsive (compact <600px mobile bottom nav, medium, expanded, wide 1240px+ detail panel). 11 responsive E2E tests. **Website** — Social proof (6 capability cards + GitHub stars). Theme toggle (localStorage + View Transitions API). Terminal demo (animated conversation replay). LCP optimization (inline critical CSS, font preload, content-visibility, deferred particles/cursor). Native popover mobile menu with `::backdrop` and `@starting-style`. **iOS** — `@Environment(\.accessibilityReduceMotion)` guards on ALL view animations (7 files). Chat empty state + sending indicator + `.thinMaterial` composer. Material hierarchy (ultraThin/thin/regular/thick). `.redacted(reason: .placeholder)` skeletons. Onboarding: illustrations, URL validation, haptics. PhosphorIcon SwiftUI component. Real gateway RPCs (sessions, tools, activity, health). Auto-fetch on tab appear. **Android** — M3 tonal surfaces (surfaceContainerLow/Container/ContainerHigh). GlassModifier. Onboarding: Phosphor illustrations, spring transitions, URL validation, notification permission. Pull-to-refresh on Overview + Sessions. Real gateway RPCs with StateFlow. **Flutter** — Deprecated in favor of native apps. **CI/Quality** — Reduced-motion E2E (verify no animations). Density screenshots at 4 breakpoints. Weekly quality-score CI job. 5 axe rule exclusions removed (link-in-text-block, scrollable-region-focusable, nested-interactive, label, aria-required-parent). Sessions view added to axe coverage.

## Action Items

- [x] **Website Performance**: Lighthouse **100**. LCP 0.8s, CLS 0.00, TBT 0ms.
- [x] **Dashboard Performance**: Lighthouse **98**. TBT 0ms, CLS 0.00. Shiki bundle optimized.
- [x] **Dashboard Visual Craft**: 8→9. 60-30-10 ratio enforced, tonal elevation, tabular-nums, sparklines.
- [x] **Native Performance**: 5→7. LazyView, deferred gateway, @Immutable, baseline profiles, const constructors.
- [x] **Native Visual Craft**: 7→8. Typography hierarchy, tonal surfaces, accent restraint, dark/light polish.
- [x] **Native Motion**: iOS 8→9, macOS/Android 7→8. springInteractive, stagger lists, predictive back.
- [x] **Android Brand**: 7→8. Phosphor vector drawables, Avenir typography throughout.
- [x] **Flutter**: Lazy loading, deferred gateway, Material 3 typography, overview density.
- [x] **Cross-surface audit**: All checks passing. Raw hex/duration violations fixed.
- [x] **C Runtime**: 8+ compile errors fixed. 5,048 tests pass, 0 ASan errors.
- [x] **Native Density**: 8→9. Animated numbers (`.contentTransition(.numericText())` iOS, `AnimatedContent` Android), message count badges, preview text, relative timestamps, collapsible Advanced sections (`DisclosureGroup`/`AnimatedVisibility`/`ExpansionTile`), sparklines on macOS.
- [x] **Native Accessibility**: 8→9. `.accessibilityLabel` on icon-only buttons, `Modifier.semantics { heading() }` on screen titles, `.accessibilityHidden(true)` on decorative elements, `.focusSection()` on macOS sidebar/detail, `stateDescription` on toggles, `clearAndSetSemantics` on status dots.
- [x] **Dashboard Visual**: Sidebar active state (left border indicator, `surfaceContainerHigh` bg, spring transition), glass card depth (dark mode `color-mix` 75%), container queries on 7 views, CSS nesting.
- [x] **Dashboard Innovation 9→10**: Anticipatory UX, ambient intelligence, vim navigation, virtual scroll, 4-breakpoint responsive, narrative motion.
- [x] **Website Innovation 9→10**: Native popover, terminal demo, social proof, theme toggle, LCP optimization.
- [x] **iOS 58→63**: Real gateway RPCs, glass depth, skeletons, reduce-motion guards, onboarding polish.
- [x] **Android 57→63**: Tonal surfaces, glass modifier, real data + pull-to-refresh, notification permission, spring onboarding.
- [x] **Axe rules tightened**: 5 exclusions removed, sessions view added.
- [ ] **Native Performance 9→10**: Instruments profiling, 60fps scroll verification, <100ms tap response.
- [ ] **VoiceOver/TalkBack manual testing**: Full screen reader audit on real devices.
- [ ] **color-contrast axe rule**: Fix remaining low-contrast elements (empty-state description, chat bubbles).
- [ ] **aria-required-children axe rule**: Fix empty listbox in chat session selector.
- [ ] **Native Brand 9→10**: Pixel-perfect cross-platform consistency audit.

## Related

- [Competitive Benchmarks](competitive-benchmarks.md)
- [Award-Winning Quality Criteria](standards/quality/award-criteria.md)
- [Design doc](plans/2026-03-09-competitive-intelligence-design.md)
