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

The homepage (`src/pages/index.astro`) is a 10-chapter scroll narrative with dot navigation. `chapterCount` in frontmatter controls the nav dot count (currently 10).

| Chapter | Section |
| --- | --- |
| 1 | Hero + Proof Stats (capabilities row) |
| 2 | Demo (terminal replay) |
| 3 | The Problem + Contrast |
| 4 | Crystal Grid |
| 5 | Device Spectrum + Architecture |
| 6 | HuLa (Programs, not prompts) |
| 7 | Ecosystem (Providers, Channels, Tools) |
| 8 | Terminal + Dashboard |
| 9 | Quality + Comparison |
| 10 | CTA (call to action) |

When adding or removing chapters, update `chapterCount` and renumber all subsequent `id="chapter-N"` + `hu-chapter-label` spans.

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
