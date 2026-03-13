---
title: Award-Winning Quality Criteria
updated: 2026-03-13
---

# Award-Winning Quality Criteria

> Concrete, measurable criteria that define award-winning quality for every Human surface. This document codifies what "best in class" means and provides the roadmap from current state to award tier.

## Award Programs & What They Measure

| Program                   | Focus                                                                 | Relevance to Human                   |
| ------------------------- | --------------------------------------------------------------------- | ------------------------------------ |
| **Awwwards**              | Design, usability, creativity, content                                | Website + dashboard — primary target |
| **CSS Design Awards**     | CSS innovation, UX, UI design                                         | Website — CSS craft showcase         |
| **FWA**                   | Forward-thinking digital design                                       | Innovation + motion                  |
| **Apple Design Awards**   | Delight, interaction, social impact, inclusivity, innovation, visuals | iOS + macOS apps                     |
| **Google Play Best Apps** | Design, UX, innovation, social impact                                 | Android app                          |
| **Webby Awards**          | Visual design, UX, functionality, content, innovation                 | Website + dashboard                  |

## Current State vs Award Tier

### Website (54/70 → Target: 63+)

| Dimension     | Current | Award Tier (9)                                                                                                                                       | Gap                                                     | Priority Fix                                      |
| ------------- | ------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------- | ------------------------------------------------- |
| Performance   | 8       | Lighthouse 99+, LCP <0.3s, CLS 0.00, INP <50ms, TTFB <100ms                                                                                          | LCP too high (2.6s), need 99+ Lighthouse                | Critical-path CSS, image CDN, edge deployment     |
| Visual Craft  | 8       | Zero raw hex/px, editorial typography with 5+ weights, tonal surface depth, micro-gradients on interactive elements, dark/light pixel-perfect parity | Close but needs polish                                  | Typography audit, gradient refinement             |
| Motion        | 7       | Spring physics on every interaction, GSAP ScrollTrigger with parallax, page transition choreography, 3D transforms on hero, scroll-driven narrative  | Missing scroll narrative, limited parallax              | Add scroll-driven hero, parallax sections         |
| Density       | 7       | Direct-label charts, animated counters, progressive disclosure, small multiples for comparisons, tabular figures                                     | Missing small multiples and direct labels               | Redesign comparison section                       |
| Accessibility | 9       | Lighthouse 100, axe zero violations, VoiceOver tested, prefers-contrast support, skip links, ARIA landmarks                                          | Almost there                                            | prefers-contrast, ARIA landmark audit             |
| Brand         | 8       | Avenir everywhere, Phosphor everywhere, 60-30-10 validated, glass system consistent, design token parity with dashboard                              | Close                                                   | Cross-surface visual audit                        |
| Innovation    | 7       | View Transitions, scroll-driven animations, @starting-style, native popover, anchor positioning, WebGPU or Three.js hero                             | Missing WebGL/3D element, limited @starting-style usage | Add 3D hero element, adopt more platform features |

### Dashboard (56/70 → Target: 63+)

| Dimension     | Current | Award Tier (9)                                                                                                               | Gap                                 | Priority Fix                       |
| ------------- | ------- | ---------------------------------------------------------------------------------------------------------------------------- | ----------------------------------- | ---------------------------------- |
| Performance   | 8       | Bundle <80KB, TTI <1s, no layout thrashing, requestIdleCallback for deferred work                                            | Need to measure and optimize        | Lighthouse audit                   |
| Visual Craft  | 8       | Tonal surfaces on every card, micro-interactions on every input, consistent elevation hierarchy, editorial stat presentation | Need more tonal variety             | Elevation audit                    |
| Motion        | 8       | Spring on all, view transitions between all routes, scroll-driven lists, ambient loading indicators                          | Close — needs ambient indicators    | Add subtle ambient animation       |
| Density       | 7       | Sparklines everywhere, animated numbers, direct-label charts, progressive disclosure on all settings                         | Missing sparklines in several views | Add sparklines to overview/usage   |
| Accessibility | 9       | 100 axe, full keyboard, focus trap, reduced-motion                                                                           | Almost there                        | Final axe audit                    |
| Brand         | 8       | Token parity with website, identical component styling                                                                       | Close                               | Cross-surface audit                |
| Innovation    | 8       | Command palette, glass tooltips, scroll-driven, view transitions                                                             | Close — needs spatial/ambient       | Add ambient intelligence indicator |

### iOS App (41/70 → Target: 63+)

| Dimension     | Current | Award Tier (9)                                                                        | Gap                                   | Priority Fix                      |
| ------------- | ------- | ------------------------------------------------------------------------------------- | ------------------------------------- | --------------------------------- |
| Performance   | 5       | 60fps scrolling, <100ms tap response, memory <50MB                                    | Untested                              | Profile and optimize              |
| Visual Craft  | 6       | HUTokens everywhere, SF Symbols + Phosphor, tonal backgrounds, editorial typography   | Missing tonal surfaces, limited views | Add overview view, tonal surfaces |
| Motion        | 6       | HUTokens.springExpressive on all transitions, matchedGeometryEffect, hero transitions | Missing matched geometry              | Add hero transitions              |
| Density       | 6       | Compact chat bubbles, inline stats, progressive settings                              | Only 2 views                          | Add overview with stats           |
| Accessibility | 6       | VoiceOver labels, Dynamic Type, high contrast                                         | Missing Dynamic Type                  | Add Dynamic Type support          |
| Brand         | 7       | Avenir, accent colors, spring physics                                                 | Close for existing views              | Extend to new views               |
| Innovation    | 5       | Live Activities, App Intents, StoreKit                                                | None implemented                      | Add App Intents for Siri          |

### Android App (35/70 → Target: 63+)

| Dimension | Current | Award Tier (9)                                                                      | Gap          | Priority Fix              |
| --------- | ------- | ----------------------------------------------------------------------------------- | ------------ | ------------------------- |
| All       | 4-6     | Full Compose UI with Material3 + HUTokens overlay, spring animations, accessibility | No UI at all | Build complete Compose UI |

## Concrete Award-Winning Checklist

### Awwwards Submission Checklist (Website)

- [ ] Lighthouse Performance >= 98 (production URL)
- [ ] Lighthouse Accessibility = 100
- [ ] Zero CLS on all pages
- [ ] LCP < 1.0s on 4G connection
- [ ] Spring physics on every interactive element
- [ ] Scroll-driven narrative with parallax
- [ ] At least one 3D/WebGL element (hero or transition)
- [ ] @starting-style enter animations on dynamically added elements
- [ ] View Transitions API between pages (if MPA) or routes (if SPA)
- [ ] Custom cursor or hover interactions that delight
- [ ] Responsive from 320px to 2560px with fluid typography
- [ ] Dark mode with equal visual quality to light mode
- [ ] prefers-reduced-motion fully supported
- [ ] prefers-contrast: more supported
- [ ] Original typography treatment (not default system stack)
- [ ] No stock photography — all illustrations/assets bespoke
- [ ] Schema.org structured data
- [ ] OG + Twitter Card meta complete with images

### Apple Design Award Checklist (iOS/macOS)

- [ ] Uses platform conventions (NavigationStack, TabView, .sheet)
- [ ] Dynamic Type support (all text scales)
- [ ] VoiceOver labels on every interactive element
- [ ] Haptic feedback on meaningful interactions
- [ ] Spring animations (UISpringTimingParameters or SwiftUI .spring)
- [ ] Widget / Live Activity / App Intent integration
- [ ] Proper light/dark/tinted appearance support
- [ ] Keyboard/trackpad support (macOS)
- [ ] Avenir custom font with proper weight mapping

### Google Play Best App Checklist (Android)

- [ ] Material 3 theming with dynamic color support
- [ ] Compose-first UI
- [ ] Edge-to-edge with proper insets
- [ ] Predictive back gesture support
- [ ] Accessibility Scanner zero issues
- [ ] Spring animations (Compose animation-core)
- [ ] Widget integration
- [ ] Proper dark theme

## Stretch Targets (Score 10)

| Dimension     | Score 10 Criteria                                                              |
| ------------- | ------------------------------------------------------------------------------ |
| Performance   | Lighthouse 100, LCP <0.2s, zero-JS critical path, streaming SSR                |
| Visual Craft  | Competitors study your UI; featured in design publications                     |
| Motion        | Indistinguishable from native on every platform; custom physics engine         |
| Density       | Zero learning curve; understanding is involuntary (Tufte's "data that speaks") |
| Accessibility | WCAG AAA, cognitive accessibility, neurodiversity considered                   |
| Brand         | Platform Transcendence — best-designed app on every device/OS                  |
| Innovation    | Defining new interaction paradigms that others adopt                           |

## Measurement Automation

All criteria marked as "automated" should be enforced in CI:

- Lighthouse scores: `.lighthouserc.json` thresholds
- axe-core violations: `accessibility.spec.ts` E2E test
- Bundle size: `check-bundle-size.sh`
- Token compliance: `lint:tokens` + `lint-raw-colors.sh`
- Component quality: `check:components`
- CLS/LCP/INP: Lighthouse CI assertions

Manual criteria require quarterly audit per `docs/standards/quality/ceremonies.md`.
