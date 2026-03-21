---
title: Accessibility Testing
updated: 2026-03-13
---

# Accessibility Testing

Testing methodology and acceptance criteria for accessibility across all human surfaces.

**Cross-references:** [ux-patterns.md](ux-patterns.md), [visual-standards.md](visual-standards.md), [../quality/ceremonies.md](../quality/ceremonies.md)

---

## 1. Compliance Targets

| Level      | Standard            | Target                          | Gate                   |
| ---------- | ------------------- | ------------------------------- | ---------------------- |
| Minimum    | WCAG 2.2 AA         | All surfaces                    | CI blocks on violation |
| Stretch    | WCAG 2.2 AAA        | Text contrast, focus indicators | Tracked, not blocking  |
| Lighthouse | Accessibility score | >= 98                           | CI blocks below 95     |
| axe-core   | Zero violations     | All E2E test pages              | CI blocks on any       |

## 2. Automated Testing

### 2.1 Lighthouse Accessibility (CI)

Every PR runs Lighthouse against the dashboard and website builds. Thresholds:

- Dashboard: >= 98 (`.lighthouserc-dashboard.json`)
- Website: >= 98 (`.lighthouserc.json`)

### 2.2 axe-core in E2E (CI)

Playwright E2E tests inject axe-core on every view:

```typescript
import AxeBuilder from "@axe-core/playwright";
const results = await new AxeBuilder({ page }).analyze();
expect(results.violations).toEqual([]);
```

Coverage: all 17 dashboard views, all website pages.

### 2.3 Color Contrast Validation

- `scripts/lint-raw-colors.sh` ensures no raw hex values bypass the token system
- Design tokens define contrast-safe color pairs
- `prefers-contrast: more` mode tested in E2E

## 3. Manual Testing Protocol

### 3.1 Screen Reader Testing

Test each surface quarterly with its primary screen reader:

| Surface   | Screen Reader    | OS                            |
| --------- | ---------------- | ----------------------------- |
| Website   | VoiceOver        | macOS Safari                  |
| Dashboard | VoiceOver + NVDA | macOS Safari + Windows Chrome |
| iOS       | VoiceOver        | iOS                           |
| macOS     | VoiceOver        | macOS                         |
| Android   | TalkBack         | Android                       |

#### Test Checklist Per View

- [ ] All interactive elements announced with role and label
- [ ] Heading hierarchy is logical (h1 > h2 > h3, no skips)
- [ ] Form inputs have associated labels (not just placeholder)
- [ ] Dynamic content changes announced via `aria-live` or `role="status"`
- [ ] Modal dialogs trap focus and announce on open
- [ ] Dismiss with Escape key works and announces return to trigger
- [ ] Images have meaningful alt text or are marked decorative
- [ ] Data tables have header associations

### 3.2 Keyboard Navigation Testing

Every view must be fully operable without a mouse:

| Action            | Key           | Expected                                    |
| ----------------- | ------------- | ------------------------------------------- |
| Navigate forward  | Tab           | Focus moves to next interactive element     |
| Navigate backward | Shift+Tab     | Focus moves to previous interactive element |
| Activate          | Enter / Space | Triggers click action                       |
| Dismiss           | Escape        | Closes modal/popover, returns focus         |
| Navigate lists    | Arrow keys    | Moves within list/menu items                |
| Skip to content   | Tab (first)   | Skip link jumps to main content             |

### 3.3 Reduced Motion Testing

- Enable `prefers-reduced-motion: reduce` in OS settings
- Verify: all spring animations disabled or replaced with instant transitions
- Verify: no content depends solely on animation for comprehension

### 3.4 High Contrast Testing

- Enable `prefers-contrast: more` in OS settings (or `forced-colors: active` on Windows)
- Verify: all text readable, focus indicators visible, interactive elements distinguishable

## 4. Native App Accessibility

### 4.1 iOS / macOS

- All custom views have `.accessibilityLabel`, `.accessibilityHint`, `.accessibilityTraits`
- Decorative elements use `.accessibilityHidden(true)`
- Screen titles use `.accessibilityAddTraits(.isHeader)`
- Dynamic values use `.accessibilityValue` for state (e.g., toggle on/off)
- Support Dynamic Type: all text uses `.font()` with `relativeTo:` parameter
- Test with VoiceOver rotor: headings, links, and landmarks navigable

### 4.2 Android

- All interactive Composables have `contentDescription` or `Modifier.semantics { }`
- Screen titles use `Modifier.semantics { heading() }`
- Decorative elements use `Modifier.clearAndSetSemantics { }`
- Toggle states use `Modifier.semantics { stateDescription = "..." }`
- Support `ANIMATOR_DURATION_SCALE = 0` (reduced motion)
- Test with TalkBack + Switch Access

## 5. Acceptance Criteria

A surface passes accessibility review when:

1. Lighthouse Accessibility >= 98
2. axe-core: zero violations on all tested pages
3. Full keyboard navigation works on every view
4. Screen reader announces all content meaningfully
5. `prefers-reduced-motion` respected
6. `prefers-contrast: more` supported
7. No color-only information encoding

## Normative References

| ID           | Source                                   | Version          | Relevance                                |
| ------------ | ---------------------------------------- | ---------------- | ---------------------------------------- |
| [WCAG22]     | W3C Web Content Accessibility Guidelines | 2.2 (2023-10-05) | Minimum AA compliance target             |
| [ARIA-APG]   | W3C ARIA Authoring Practices Guide       | 1.2 (2023)       | Widget patterns and keyboard interaction |
| [axe-rules]  | Deque axe-core Rule Descriptions         | 4.x              | Automated violation detection rules      |
| [APCA]       | Advanced Perceptual Contrast Algorithm   | 0.1.9            | Contrast measurement (future adoption)   |
| [EN-301-549] | European Accessibility Standard          | 3.2.1 (2021)     | EU regulatory compliance                 |
