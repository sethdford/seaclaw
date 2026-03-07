---
title: Design Token Audit Report
---

# Design Token Audit Report

**Date:** 2026-03-07  
**Scope:** All consumers — ui, website, docs, design-tokens (excluding node_modules, dist, \_tokens.css)

## Summary

| Metric                               | Count |
| ------------------------------------ | ----- |
| **Total tokens defined**             | 367   |
| **Tokens used** (direct or indirect) | 274   |
| **Truly unused**                     | 173   |

## Categorization

### 1. Truly Unused (173 tokens)

These tokens are **not referenced** in ui/src, website/src, docs, or design-tokens. They are also **not used as values** in other tokens (e.g. `var(--sc-x)` inside another token's definition).

**Note:** Base tokens in `base.tokens.json` (e.g. `color.neutral.50`) are **not** emitted as CSS vars — they are resolved into semantic tokens during build. They do not appear in this audit.

### 2. Used Indirectly (1 token)

- `--sc-accent-subtle` — referenced inside `--sc-shadow-glow-accent`, which is used in `website/src/styles/global.css`

### 3. Platform-Specific Consumers

- **C** (`include/seaclaw/design_tokens.h`): Uses `SC_COLOR_*` macros — hand-maintained, not generated from CSS var names.
- **Apps** (Flutter, iOS, Android): Use generated `SCTokens` / `DesignTokens` — same source JSON, different output format. Removing a token from source would affect all platforms.

## Full List of Truly Unused Tokens

### Avatar (5)

- `--sc-avatar-font-size`, `--sc-avatar-font-weight`
- `--sc-avatar-size-lg`, `--sc-avatar-size-md`, `--sc-avatar-size-sm`

### Blur (2)

- `--sc-blur-lg`, `--sc-blur-xl`

### Breakpoints (5)

- `--sc-breakpoint-2xl`, `--sc-breakpoint-lg`, `--sc-breakpoint-md`, `--sc-breakpoint-sm`, `--sc-breakpoint-xl`

### Choreography (1)

- `--sc-cascade-max`

### Command Palette (6)

- `--sc-command-palette-item-padding-x`, `--sc-command-palette-item-padding-y`, `--sc-command-palette-item-radius`
- `--sc-command-palette-max-width`, `--sc-command-palette-padding`, `--sc-command-palette-radius`

### Container Transform (5)

- `--sc-container-transform-duration`, `--sc-container-transform-easing`, `--sc-container-transform-fade-through-duration`
- `--sc-container-transform-scale-end`, `--sc-container-transform-scale-start`

### Dropdown (1)

- `--sc-dropdown-radius`

### Elevation (4)

- `--sc-elevation-0`, `--sc-elevation-2`, `--sc-elevation-4`, `--sc-elevation-5`

### Motion — Emphasize (2)

- `--sc-emphasize-duration`, `--sc-emphasize-easing`

### Motion — Enter/Exit (4)

- `--sc-enter-duration`, `--sc-enter-easing`, `--sc-exit-duration`, `--sc-exit-easing`

### Floating Action Button (3)

- `--sc-floating-action-button-icon-size`, `--sc-floating-action-button-radius`, `--sc-floating-action-button-size`

### Glass (9)

- `--sc-glass-dynamic-light-ambient`, `--sc-glass-dynamic-light-angle`
- `--sc-glass-interactive-press-saturate-boost`
- `--sc-glass-prominent-refraction-scale`, `--sc-glass-standard-refraction-scale`
- `--sc-glass-subtle-inset-opacity`, `--sc-glass-subtle-refraction-scale`
- `--sc-glass-vibrancy-backdrop-brightness`, `--sc-glass-vibrancy-icon-boost`, `--sc-glass-vibrancy-text-boost`

### Input (4)

- `--sc-input-font-size`, `--sc-input-padding-x`, `--sc-input-padding-y`, `--sc-input-radius`

### Motion — Move (2)

- `--sc-move-duration`, `--sc-move-easing`

### Opacity (7)

- `--sc-opacity-dragged`, `--sc-opacity-focus`, `--sc-opacity-hover`
- `--sc-opacity-overlay-heavy`, `--sc-opacity-overlay-light`, `--sc-opacity-overlay-medium`, `--sc-opacity-pressed`

### Motion — Page (4)

- `--sc-page-enter-duration`, `--sc-page-enter-easing`, `--sc-page-exit-duration`, `--sc-page-exit-easing`

### Path Motion (4)

- `--sc-path-motion-arc-duration`, `--sc-path-motion-arc-easing`
- `--sc-path-motion-offset-distance-end`, `--sc-path-motion-offset-distance-start`

### Physics (6)

- `--sc-physics-dropdown-anticipate-scaleY`, `--sc-physics-modal-anticipate-pause`
- `--sc-physics-secondary-damping`, `--sc-physics-secondary-delay`
- `--sc-physics-toggle-squash-scaleX`, `--sc-physics-toggle-squash-scaleY`

### Progress (2)

- `--sc-progress-height`, `--sc-progress-radius`

### Shadow (1)

- `--sc-shadow-inset`

### Motion — Shared Element (2)

- `--sc-shared-element-duration`, `--sc-shared-element-easing`

### Sheet (4)

- `--sc-sheet-handle-height`, `--sc-sheet-handle-width`, `--sc-sheet-padding`, `--sc-sheet-radius`

### Spring (8)

- `--sc-spring-dramatic-damping`, `--sc-spring-dramatic-stiffness`
- `--sc-spring-expressive-damping`, `--sc-spring-expressive-stiffness`
- `--sc-spring-micro-damping`, `--sc-spring-micro-stiffness`
- `--sc-spring-standard-damping`, `--sc-spring-standard-stiffness`

### Choreography (1)

- `--sc-stagger-max`

### Surface (1)

- `--sc-surface-glow`

### Tabs (5)

- `--sc-tabs-font-size`, `--sc-tabs-font-weight`, `--sc-tabs-indicator-height`
- `--sc-tabs-padding-x`, `--sc-tabs-padding-y`

### Tooltip (5)

- `--sc-tooltip-font-size`, `--sc-tooltip-max-width`, `--sc-tooltip-padding-x`, `--sc-tooltip-padding-y`, `--sc-tooltip-radius`

### Type Roles — Body (12)

- `--sc-type-body-lg-*`, `--sc-type-body-md-*`, `--sc-type-body-sm-*` (size, weight, letter-spacing, line-height each)

### Type Roles — Caption (4)

- `--sc-type-caption-size`, `--sc-type-caption-weight`, `--sc-type-caption-letter-spacing`, `--sc-type-caption-line-height`

### Type Roles — Display (8)

- `--sc-type-display-lg-*`, `--sc-type-display-md-*` (size, weight, letter-spacing, line-height each)

### Type Roles — Headline (12)

- `--sc-type-headline-lg-*`, `--sc-type-headline-md-*`, `--sc-type-headline-sm-*` (size, weight, letter-spacing, line-height each)

### Type Roles — Label (12)

- `--sc-type-label-lg-*`, `--sc-type-label-md-*`, `--sc-type-label-sm-*` (size, weight, letter-spacing, line-height each)

### Type Roles — Title (12)

- `--sc-type-title-lg-*`, `--sc-type-title-md-*`, `--sc-type-title-sm-*` (size, weight, letter-spacing, line-height each)

### View Transition (4)

- `--sc-view-transition-duration`, `--sc-view-transition-easing`
- `--sc-view-transition-new-enter`, `--sc-view-transition-old-exit`

### Z-Index (5)

- `--sc-z-base`, `--sc-z-modal`, `--sc-z-sticky`, `--sc-z-toast`, `--sc-z-tooltip`

## Is It Safe to Remove?

**Before removing any token from the source JSON files:**

1. **Apps dependency:** Flutter, iOS, and Android consume the same `design-tokens/*.json` source. Removing a token will affect generated `SCTokens` / `DesignTokens` in those platforms. Verify app usage first (e.g. `SCTokens.avatarSizeLg` in Swift, `SCTokens.avatar_size_lg` in Dart).

2. **Reserved for future use:** Tokens like `--sc-z-modal`, `--sc-z-toast`, `--sc-z-tooltip` may be intended for components not yet built. Check roadmap/design docs.

3. **Type roles:** The `--sc-type-*` tokens are expanded typography roles. Components might use `--sc-text-base` or `--sc-font` instead of the granular type-body-lg-size. Consider whether to adopt type roles in components or remove them.

4. **Recommended approach:** Remove tokens in small batches, run the design-tokens build, then run app builds and tests to catch breakages.

## Audit Script

The script `ui/scripts/audit-unused-tokens-all-consumers.sh` performs this audit. It was updated to exclude `node_modules`, `dist`, and `_tokens.css` from the search for accuracy and speed.
