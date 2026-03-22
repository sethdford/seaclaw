---
title: Design asset SVGs
description: Shared SVG files for docs HTML demos and pointers to ui/website public assets.
---

# Design asset SVGs

Static SVGs shared by **standalone HTML** under `docs/*.html` (opened from disk or a simple static server).

| File | Purpose |
|------|---------|
| `noise-grain.svg` | Film-grain texture for mood board / explorer demos (`url("design-assets/noise-grain.svg")`). |
| [`summer-web-palette.md`](./summer-web-palette.md) | How **Web Color** / Artboard-style palettes map to `color.viz-extended` and chart tokens 9–16. |

**Dashboard** and **marketing site** use their own copies so builds stay self-contained:

- `ui/public/noise-grain.svg` — referenced from `ui/src/styles/theme.css` as `/noise-grain.svg`
- `website/public/noise-grain.svg` — referenced from `website/src/styles/global.css` as `/noise-grain.svg`

Keep the three files in sync when tuning the filter (e.g. `baseFrequency`, `numOctaves`).

**Icons** in the Lit app remain inline Phosphor paths in [`ui/src/icons.ts`](../../ui/src/icons.ts). **Wordmark** lives at [`ui/src/assets/logo.svg`](../../ui/src/assets/logo.svg) and is imported into the sidebar via `?raw`.
