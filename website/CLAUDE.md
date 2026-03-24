# website/ — Marketing Site

Astro static site at h-uman.ai.

## Pages

```
src/pages/index.astro   Homepage
src/pages/brand.astro   Brand guidelines
src/pages/design.astro  Design system showcase
src/pages/404.astro     Not found page
```

## Homepage Structure

The homepage (`src/pages/index.astro`) is a 15-chapter scroll narrative with dot navigation. `chapterCount` in frontmatter controls the nav dot count.

| Chapter | Section |
| --- | --- |
| 1 | Capabilities (glass tile grid) |
| 2 | Benchmarks |
| 3 | Persona |
| 4 | Memory |
| 5 | BTH (beyond the human) |
| 6 | Intelligence |
| 7 | Security |
| 8 | Architecture |
| 9 | **HuLa** (Programs, Not Prompts) |
| 10 | Ecosystem (providers/channels) |
| 11 | Terminal |
| 12 | Dashboard preview |
| 13 | Quality (ring gauges) |
| 14 | Comparison |
| 15 | CTA (call to action) |

When adding or removing chapters, update `chapterCount` and renumber all subsequent `id="chapter-N"` + `hu-chapter-label` spans.

## Rules

- Font: Avenir via `var(--hu-font)`. Never import Google Fonts.
- Icons: inline Phosphor SVGs with `viewBox="0 0 256 256" fill="currentColor"`.
- Colors: `--hu-*` CSS custom properties only. No raw hex values.
- Spacing/radius: `--hu-space-*` and `--hu-radius-*` tokens only.
- Animation: `--hu-duration-*` and `--hu-ease-*` tokens. Respect `prefers-reduced-motion`.
- Accessibility: WCAG 2.1 AA minimum (4.5:1 text contrast, 3:1 UI contrast).

## Commands

```bash
npm run check        # astro check
npm run dev          # dev server
npm run build        # production build
npm run format:check # prettier
```
