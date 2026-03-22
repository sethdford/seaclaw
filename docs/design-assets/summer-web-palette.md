---
title: Web Color / summer.st palette reference
description: Maps extended chart primitives (color.viz-extended) to the broader marketing palette in Artboard 4.svg.
---

# Web Color palette reference

The **Web Color** / **summer.st** artboard (e.g. `Artboard 4.svg` in Downloads) encodes a wide set of greens, blues, oranges, yellows, purples, and neutrals. Human does **not** mirror every swatch as UI chrome; instead we:

1. Keep **brand UI** on `color.human`, `color.blue`, `color.amber`, `color.coral`, `color.teal`, and neutrals.
2. Expose a small **`color.viz-extended`** group in `design-tokens/base.tokens.json` for **data visualization and illustration** only.
3. Wire **chart categorical 9–16** to those primitives in `design-tokens/data-viz.tokens.json` (→ `--hu-chart-categorical-9` … `16`).

## `color.viz-extended` ↔ representative artboard hexes

| Token path | Hex | Notes |
|------------|-----|--------|
| `color.viz-extended.forest` | `#00703C` | Deep greens on artboard |
| `color.viz-extended.cyan` | `#009BDE` | Bright cyans (`#009BDE`, `#0095D3`, …) |
| `color.viz-extended.violet` | `#9D61CC` | Purple series tile |
| `color.viz-extended.chartreuse` | `#C2CD23` | Lime / chartreuse band |
| `color.viz-extended.gold` | `#F0D202` | Strong yellow |
| `color.viz-extended.orange` | `#FF6800` | Vivid orange |
| `color.viz-extended.crimson` | `#CC0000` | Strong red — **chart series only** (not `--hu-error`) |
| `color.viz-extended.azure` | `#2B89CB` | Sky azure |

To adopt additional artboard colors later, add named entries under `color.viz-extended` (or a new `viz-*` group) and reference them from `chart.categorical.*` or `data-viz` ramps — then run `npx tsx design-tokens/build.ts`.

## Figma / Tokens Studio

Import [`docs/tokens-studio.json`](../tokens-studio.json): **`base.color.viz-extended`** matches the chart-only primitives, and **`data-viz.chart.categorical.1`–`16`** lists resolved hex values for swatches and styles (mirror of `data-viz.tokens.json` after build).
