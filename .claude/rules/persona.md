---
paths: src/persona/**/*.c, src/persona/**/*.h, include/human/persona/**/*.h
---

# Persona System Rules

Manages agent identity, communication style, and behavioral adaptation. Read `src/persona/CLAUDE.md` and `AGENTS.md` section 7.6 before modifying.

## JSON Schema Validation

- Persona JSON must validate against schema: `include/human/persona/schema.json`
- Missing/empty fields must be handled gracefully (sensible defaults)
- Extra fields must be preserved (for extensibility)
- Overlay lookup is case-insensitive by channel name
- Contact profiles must include relationship stage, interaction count, history

## Profile Loading

- Load from `~/.human/personas/<name>.json` (user personas)
- Load from `<config>/personas/` (project personas)
- Markdown loader (`markdown_loader.c`) parses YAML frontmatter + body
- All allocations must be freed via `hu_persona_deinit`
- `HU_IS_TEST` guards on file I/O

## Per-Channel Overlays

- Overlays override: formality, length, emoji, typing_quirks, message_splitting
- Channel name lookup: case-insensitive, exact match preferred
- Fallback chain: channel-specific → general → defaults
- Per-contact overlays stack on top of channel overlays

## Example Banks

- Examples grouped by channel and topic
- Example selection prefers topic-relevant, fallback to general
- Each example: `{ context, incoming_message, response }`
- Used for few-shot prompting in `hu_persona_build_prompt`
- Max examples per selection: configurable (default 5)

## Prompt Assembly

- `hu_persona_build_prompt` combines persona + overlay + examples + context
- Order: identity → traits → rules → channel overlay → examples → mood/circadian
- Voice parameters (tone, pace, pitch) for voice channels
- Humor timing rules for social channels

## Style Learning & Feedback

- Style learner (`style_learner.c`) must not drift toward harmful patterns
- Feedback loop requires policy gate (anti-sycophancy, safety rails)
- Mood/circadian shifts must be bounded (sanity checks, not > 2x normal range)
- Temporal changes (seasonal, event-based) must be configurable

## Validation

```bash
./human_tests --suite=Persona
./human_tests --suite=PersonaMood
./human_tests --suite=CircadianPersona
```

## Standards

- Read `docs/standards/engineering/naming.md` for identifier conventions
- Read `docs/standards/ai/persona.md` for persona design principles
