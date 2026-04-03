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

## Chapter Consolidation Analysis

The normative standard (`docs/standards/design/ux-patterns.md` §2.7) caps scroll narratives at 7 chapters. The current 15-chapter structure exceeds this. Award judges typically won't scroll through 15 sections.

**Recommended consolidation path (15 → 9 chapters):**

| New # | Content | From |
| --- | --- | --- |
| 1 | Hero + Proof Stats (capabilities row) | Hero + Ch1 + Ch3 |
| 2 | Demo (terminal replay) | Ch2 |
| 3 | The Problem + Contrast | Ch4 + Ch5 |
| 4 | Architecture + Crystal Grid | Ch6 + Ch8 |
| 5 | Device Spectrum | Ch7 |
| 6 | HuLa + Ecosystem | Ch9 + Ch10 |
| 7 | Terminal + Dashboard | Ch11 + Ch12 |
| 8 | Quality + Comparison | Ch13 + Ch14 |
| 9 | CTA | Ch15 |

This preserves all content while tightening the narrative. The capabilities grid (Ch1) naturally fits in the hero, stats (Ch3) can be a hero sub-row, and contrast (Ch5) is a natural suffix to the dependency problem (Ch4). Requires updating `chapterCount`, re-numbering all `id="chapter-N"` and `hu-chapter-label` spans.

## Rules

- Font: Avenir via `var(--hu-font)`. Never import Google Fonts.
- Icons: inline Phosphor SVGs with `viewBox="0 0 256 256" fill="currentColor"`.
- Colors: `--hu-*` CSS custom properties only. No raw hex values.
- Spacing/radius: `--hu-space-*` and `--hu-radius-*` tokens only.
- Animation: `--hu-duration-*` and `--hu-ease-*` tokens. Respect `prefers-reduced-motion`.
- Accessibility: WCAG 2.1 AA minimum (4.5:1 text contrast, 3:1 UI contrast).

## Award Submission Checklist

Once the site is deployed and scoring well on Lighthouse, submit to:

1. **Awwwards** (https://awwwards.com/submit/) — ~$69, rolling, needs 8.5+ on Design/Usability/Creativity/Content
2. **CSS Design Awards** (https://cssdesignawards.com/submit/) — rolling, scored on UI (40%), UX (30%), Innovation (30%)
3. **FWA** (https://thefwa.com/submit/) — rolling, creativity-first judging
4. **Web Marketing Association** — deadline May 29, 2026

Pre-submission checklist:
- [ ] Deploy to `h-uman.ai` via GitHub Pages
- [ ] Lighthouse Performance >= 95, Accessibility >= 98, Best Practices >= 95, SEO >= 95
- [ ] Test on real devices (iPhone SE, iPhone 15, Samsung Galaxy, iPad)
- [ ] axe-core clean on all pages
- [ ] Full keyboard navigation verified
- [ ] CrUX data showing good ratings after 28 days of traffic

## Commands

```bash
npm run check        # astro check
npm run dev          # dev server
npm run build        # production build
npm run format:check # prettier
```
