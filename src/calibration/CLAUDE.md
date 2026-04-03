# src/calibration/ — Style Calibration & A/B Testing

Persona style learning, voice cloning calibration, A/B comparison testing, and timing analysis for response pacing.

## Key Files

```
calibrate.c            Core calibration loop — measure and adjust persona parameters
style_analyzer.c       Analyze writing style (sentence length, vocabulary, formality)
timing_analyzer.c      Analyze response timing patterns for natural pacing
ab_compare.c           A/B comparison framework for persona variants
clone.c                Voice clone calibration (Cartesia voice similarity scoring)
```

## Usage

Calibration runs as part of the persona feedback loop:
1. `style_analyzer` extracts features from human messages
2. `calibrate` adjusts persona parameters toward the target style
3. `ab_compare` tests alternative configurations
4. `timing_analyzer` tunes response pacing

## Rules

- `HU_IS_TEST` guards on all external API calls (Cartesia, etc.)
- A/B tests must be reproducible — seed RNG from config, not wall clock
- Style features are statistical (means, distributions) — never store raw messages
