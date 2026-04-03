# src/persona/ — Persona System (22 files)

Defines and manages agent identity, communication style, emotional state, and behavioral adaptation. Persona profiles are JSON in `~/.human/personas/`.

## Core Persona (Data Model)

```
persona.c            Loading, parsing, validating persona JSON profiles
creator.c            Interactive persona creation workflow
analyzer.c           Analyzes communication samples to extract traits
examples.c           Example conversation bank (few-shot prompting)
sampler.c            Sampling and variation in expression
feedback.c           Feedback loop for persona refinement
cli.c                CLI subcommands (list, show, create, analyze, validate)
```

## Behavior Adaptation

```
mood.c               Mood tracking and emotional state modeling
circadian.c          Circadian rhythm — time-of-day behavior shifts
relationship.c       Relationship stage tracking (new, familiar, close)
voice_maturity.c     Voice maturity evolution over time
humor.c              Humor framework (timing, style, cultural context)
```

## Style & Personalization

```
style_clone.c        Clones communication style from samples
style_learner.c      Learns style drift over time (language models, vocabulary shifts)
markdown_loader.c    Loads persona from Markdown format (alternative to JSON)
persona_fuse.c       Fuses per-channel overlays with core persona
temporal.c           Temporal persona changes (seasonal, event-based)
```

## Auto-Generation & Learning

```
auto_profile.c       Auto-generates profile from conversation history
auto_tune.c          Auto-tunes persona parameters over time
training.c           Prepares training data for fine-tuning
life_sim.c           Life simulation context (daily routine, life chapter)
replay.c             Conversation replay for persona testing/validation
```

## Key Types

```c
hu_persona_t               Full persona profile (identity, traits, vocab, rules, values, overlays, examples, contacts, motivation, humor, voice, etc.)
hu_persona_overlay_t       Per-channel overrides (formality, length, emoji, typing quirks, message splitting)
hu_persona_example_bank_t  Example conversations grouped by channel and topic
hu_persona_example_t       Single few-shot example (context, input, response)
hu_contact_profile_t       Per-contact relationship data and history
hu_persona_mood_t          Current mood state with intensity and triggers
hu_persona_circadian_t     Time-of-day behavioral shifts and energy levels
hu_persona_style_t         Communication style (word choice, pacing, formality)
```

## Prompt Assembly Flow

```
hu_persona_build_prompt
  → identity + traits + communication rules
  → channel overlay (formality, length, emoji, quirks)
  → selected examples (few-shot, topic + channel relevant)
  → contact context (relationship stage, history)
  → mood + circadian context
  → temporal/seasonal adjustments
  → voice characteristics (for voice channel)
  → humor/timing rules
  → final system prompt string
```

## Key Functions

- `hu_persona_load` — load from `~/.human/personas/<name>.json`
- `hu_persona_load_json` — parse from raw JSON
- `hu_persona_load_markdown` — parse from Markdown file
- `hu_persona_build_prompt` — compose system prompt
- `hu_persona_select_examples` — pick few-shot examples (topic/channel aware)
- `hu_persona_find_overlay` — look up channel overrides
- `hu_persona_find_contact` — look up per-contact data
- `hu_persona_feedback_record` / `hu_persona_feedback_apply` — feedback loop
- `hu_persona_sample_variation` — generate expression variants
- `hu_persona_analyze_style` — extract traits from samples
- `hu_persona_apply_mood` — modulate output based on current mood
- `hu_persona_apply_circadian` — apply time-of-day shifts

## Rules

- JSON parsing must handle missing/empty fields gracefully
- Overlay lookup by channel name — case-insensitive
- Example selection prefers topic-relevant, fallback to general
- Never hardcode persona data — always load from profile
- Use neutral test data — no real personal information
- Free all allocations via `hu_persona_deinit`
- `HU_IS_TEST` guards on file I/O (persona files)
- Style learner must not drift toward harmful patterns (via policy gate)
- Mood/circadian shifts must be bounded (sanity checks)
