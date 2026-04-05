---
title: The 12 Frontiers of Humanness
description: Design philosophy and architecture for h-uman's humanness subsystems
---

# The 12 Frontiers of Humanness

These twelve subsystems close the gap from "sophisticated AI with personality" to "genuinely feels like a person." They build on h-uman's existing persona, cognition, and memory infrastructure.

## Design Principles

1. **Grounded in psychology** — Attachment dynamics draw from Bowlby/Ainsworth. Rupture-repair follows therapeutic alliance theory. Presence gradient mirrors how real attention varies.

2. **Emergent, not scripted** — Each feature computes state from real signals (emotional intensity, message timing, relationship stage) rather than following predetermined scripts.

3. **Subtle by default** — Features produce context directives for the LLM, not rigid templates. The model decides how to express tiredness, surprise, or boundary — the system only sets the conditions.

4. **Layered cascade** — Somatic state feeds micro-expression. Presence gradient affects memory retrieval depth. Attachment shapes rupture-repair response strategy. Features compose, not compete.

5. **Respectful of silence** — Many features produce NULL output when their state is near-baseline. The system only speaks up when there's something meaningful to say.

## Architecture

Every feature follows the same pattern:

- **C struct** for state (`hu_somatic_state_t`, `hu_attachment_state_t`, etc.)
- **Compute function** that updates state each turn from real signals
- **Prompt builder** that emits a markdown directive when active
- **Integration** via `hu_prompt_config_t` fields injected in `agent_turn.c`
- **Persistence** via `hu_frontier_persist_save/load` to SQLite for cross-session continuity

All per-agent state lives in `hu_frontier_state_t` embedded in `hu_agent_t`, ensuring correct behavior across multiple agents and threads. When a persona is switched via `hu_agent_set_persona`, frontier states that derive from persona (narrative self, creative voice, boundaries) are re-initialized from the new persona's fields.

```
Per-Turn Pipeline:

  User Message
       │
  ┌────▼────┐
  │ Somatic  │── energy, social_battery, focus, arousal
  │ Compute  │
  └────┬────┘
       │
  ┌────▼────┐    ┌──────────┐    ┌──────────────┐
  │ Presence │───▶│ Micro-   │───▶│ Novelty      │
  │ Gradient │    │ Expresion│    │ Detection    │
  └────┬────┘    └──────────┘    └──────────────┘
       │
  ┌────▼────────┐  ┌──────────┐  ┌───────────────┐
  │ Attachment   │  │ Rupture- │  │ Narrative     │
  │ Dynamics     │  │ Repair   │  │ Self          │
  └──────────────┘  └──────────┘  └───────────────┘
       │
  ┌────▼────────┐  ┌──────────┐  ┌───────────────┐
  │ Growth      │  │ Creative │  │ Genuine       │
  │ Narrative   │  │ Voice    │  │ Boundaries    │
  └──────────────┘  └──────────┘  └───────────────┘
       │
       ▼
  System Prompt → LLM → Message Choreography → Channel Send
```

## The 12 Features

### F1: Somatic State Engine
Continuous physiological simulation — energy, social battery, focus, arousal. Creates authentic day-to-day variation. Energy recharges with time gaps, drains with conversation load. Social battery drains per interaction. Focus decays with topic switches.

### F2: Message Choreography Engine
Transforms single LLM responses into multi-message delivery with human rhythm. Detects reaction prefixes ("haha", "oh wait"), splits on paragraph breaks, adds typing delays proportional to segment length. Energy affects delivery — tired means fewer, shorter messages.

### F3: Autobiographical Self
Persistent narrative identity — "who I am, how I got here, what matters to me." Identity statement, recurring themes, origin stories, growth arcs, current preoccupation. Evolves from interactions over time.

### F4: Novelty Detection
Tracks what the assistant has encountered before so it can express authentic surprise at genuinely new information. Compares incoming entities against memory graph, STM topics, and a persistent seen-topic hash ring buffer (FNV-1a, 64 slots). Hashes are persisted to SQLite so novelty detection works across sessions. Cooldown prevents surprise fatigue.

### F5: Attachment Dynamics
Models Bowlby/Ainsworth attachment styles for the user. Infers secure/anxious/avoidant/disorganized patterns from message frequency, emotional sharing, gap distress, and independence signals. Warm-starts from the contact profile's `attachment_style` field when available, giving the model a head start on returning contacts. Shapes response strategy accordingly.

### F6: Rupture-Repair Cycle
Detects when something the assistant said landed wrong (tone shift, explicit correction, withdrawal). Progresses through stages: detected → acknowledged → repairing → repaired/unresolved. The `trigger_summary` is now persisted to SQLite so mid-rupture context survives across sessions. Guides the assistant to repair with specificity.

### F7: Relational Episode Memory
Tags memories with relational texture — not just "what happened" but "how did this feel between us." Episodes are automatically created at the end of emotionally significant turns (intensity > 0.5) and stored in the `relational_episodes` SQLite table. At the start of each turn, the top 5 episodes by significance are loaded for the current contact and injected into the prompt. The full read/write cycle is now complete.

### F8: Presence Gradient
Variable depth of engagement — casual for banter, deep for vulnerability. Affects memory retrieval budget (1x-3x), model tier selection, and response investment. Computed from emotional weight, vulnerability signals, and relationship depth.

### F9: Micro-Expression Text Layer
Real-time style modulation as a function of somatic and emotional state. Adjusts target length, punctuation density, emoji probability, capitalization energy, and ellipsis frequency. Tired = shorter, more ellipses. Excited = more punctuation.

### F10: Creative Voice Engine
Persona-shaped metaphors and worldview anchors. Draws from persona traits and conversation history to define natural metaphor domains and perspective lenses. Expressiveness controls how metaphorical the voice becomes.

### F11: Growth Narrative
Tracks how the relationship evolves — naming growth, recognizing milestones. Automatically detects growth signals (secure attachment formation, successful rupture repair, emotionally rich engagement) with deduplication to prevent repeated observations. Surfaces observations when topically relevant with a cooldown to avoid surveillance feel.

### F12: Genuine Boundaries
Value-driven personal stances that emerged from the persona's evolved opinions. Distinguished from safety rules: expressed as personal preference with warmth, not policy. Includes origin stories for deeper relationships.

## Persistence

Frontier state is saved to and loaded from SQLite via `hu_frontier_persist_save/load` at the boundaries of each agent turn. Persisted fields include:

- **Somatic**: energy, social_battery, focus, arousal, conversation load
- **Attachment**: style, confidence, all four dimensions (proximity, safe_haven, secure_base, separation)
- **Novelty**: cooldown, turns_since_last_surprise, seen-topic FNV-1a hashes (BLOB)
- **Rupture**: stage, severity, turns_since, trigger_summary (TEXT)
- **Growth**: observations and milestones in a separate `growth_records` table (type, text, evidence, confidence, significance, surfaced flag, timestamp)
- **Relational episodes**: `relational_episodes` table, pruned to 20 per contact

The prompt cache bypass condition in `agent_turn.c` includes all 11 frontier context variables — if any frontier computed a non-NULL directive, the static prompt cache is invalidated and the full prompt is rebuilt.

## Observability

Frontier state transitions emit `HU_OBSERVER_EVENT_FRONTIER` events with:
- `frontier`: which feature ("attachment", "rupture", "presence")
- `transition`: the new state name (e.g. "secure", "detected", "attentive")
- `value`: the confidence or score at transition time

Events fire when attachment style changes, rupture stage transitions, or presence reaches attentive/deep.

## Testing

The `humanness_frontiers` test suite (60 tests) covers:
- Unit tests for each feature's init, compute, context build, and edge cases (43 tests)
- Integration tests for cross-feature cascades (somatic→micro, presence→memory, energy→choreography)
- SQLite roundtrip tests for frontier persistence, growth persistence, rupture trigger persistence, and relational episode storage
- Multi-turn evolution test simulating 5 turns of frontier state progression with full persistence roundtrip
- Novelty seen-hash persistence across tracker instances

## Files

| Feature | Header | Source | Tests |
|---------|--------|--------|-------|
| Somatic | `include/human/persona/somatic.h` | `src/persona/somatic.c` | `test_humanness_frontiers.c` |
| Choreography | `include/human/agent/choreography.h` | `src/agent/choreography.c` | `test_humanness_frontiers.c` |
| Narrative Self | `include/human/persona/narrative_self.h` | `src/persona/narrative_self.c` | `test_humanness_frontiers.c` |
| Novelty | `include/human/cognition/novelty.h` | `src/cognition/novelty.c` | `test_humanness_frontiers.c` |
| Attachment | `include/human/cognition/attachment.h` | `src/cognition/attachment.c` | `test_humanness_frontiers.c` |
| Rupture-Repair | `include/human/cognition/rupture_repair.h` | `src/cognition/rupture_repair.c` | `test_humanness_frontiers.c` |
| Relational Episode | `include/human/memory/relational_episode.h` | `src/memory/relational_episode.c` | `test_humanness_frontiers.c` |
| Presence | `include/human/cognition/presence.h` | `src/cognition/presence.c` | `test_humanness_frontiers.c` |
| Micro-Expression | `include/human/persona/micro_expression.h` | `src/persona/micro_expression.c` | `test_humanness_frontiers.c` |
| Creative Voice | `include/human/persona/creative_voice.h` | `src/persona/creative_voice.c` | `test_humanness_frontiers.c` |
| Growth Narrative | `include/human/agent/growth_narrative.h` | `src/agent/growth_narrative.c` | `test_humanness_frontiers.c` |
| Genuine Boundaries | `include/human/persona/genuine_boundaries.h` | `src/persona/genuine_boundaries.c` | `test_humanness_frontiers.c` |
| Frontier Persist | `include/human/agent/frontier_persist.h` | `src/agent/frontier_persist.c` | `test_humanness_frontiers.c` |
