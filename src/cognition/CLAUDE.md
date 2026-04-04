# src/cognition/ — Cognitive Processing Pipeline

Cognitive subsystems modeling human-like reasoning: dual-process thinking, emotional processing, episodic memory formation, metacognition, trust calibration, and skill routing.

## Key Files

```
dual_process.c         System 1 (fast/intuitive) vs System 2 (slow/deliberate) routing
emotional.c            Emotional state tracking and influence on decisions
episodic.c             Episodic memory formation from conversation events
metacognition.c        Self-monitoring: confidence estimation, uncertainty detection
trust.c                Trust calibration per contact/channel
skill_routing.c        Route requests to appropriate cognitive skills
evolving.c             Evolving personality/preference state
db.c                   Cognition persistence (SQLite)
```

## Headers

`include/human/cognition/` — `dual_process.h`, `emotional.h`, `episodic.h`, `metacognition.h`, `db.h`

## Rules

- `HU_IS_TEST` guards on all SQLite and network operations
- Cognition modules are stateless processors — state lives in `db.c`
- Trust scores are per-contact, per-channel — never global
