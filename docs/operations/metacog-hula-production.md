---
title: Metacognition and HuLa in production
description: Tuning knobs, telemetry, local CI parity, and operational checklist
updated: 2026-03-21
---

# Metacognition and HuLa in production

This note complements [`metacog-analytics.md`](./metacog-analytics.md) (SQLite queries) and [`docs/api/config.md`](../api/config.md) (schema). Use it when rolling out metacognition + HuLa beyond dev laptops.

## Configuration knobs

| Surface | What to tune |
|--------|----------------|
| **`agent.metacognition` in config** | `enabled`, `confidence_threshold`, `coherence_threshold`, `repetition_threshold`, `max_reflects`, `max_regen`, `hysteresis_min`, `use_calibrated_risk`, weights (`w_*`), `risk_high_threshold`. |
| **Environment** | `HUMAN_METACOGNITION`, `HUMAN_METACOG_MAX_REGEN`, `HUMAN_METACOG_LOGPROBS` (provider support required). Reload the process after changing `.env`. |
| **HuLa** | `agent.hula` in config (default on). Operator-oriented guide: [`docs/guides/hula.md`](../guides/hula.md). HuLa runs only in non-test binaries; tool batches use policy + observer hooks on the HuLa executor path. |

**Spawn parity:** Named delegates (`delegate` tool → `hu_agent_pool_spawn_named`) inherit the parent’s metacognition policy pointer via `hu_spawn_config_apply_current_tool_agent()` so fleet spawns match `/spawn` and `agent_spawn` behavior.

## Telemetry

| Signal | Where |
|--------|--------|
| **Row-level metacog** | `metacog_history` in cognition DB (see metacog-analytics doc). |
| **BTH counters** | `metacog_*`, `hula_tool_turns` — exposed via admin/gateway metrics and `hu_bth_metrics_summary`. |
| **HuLa counter hook** | `hu_bth_metrics_record_hula_tool_turn()` increments `hula_tool_turns` (used from `agent_turn` on successful HuLa execution; tests call the same API for regression coverage). |

## Tuning workflow (suggested)

1. Start with **metacognition enabled** and **default thresholds**; watch `metacog_hysteresis_suppressed` vs `metacog_regens`.
2. If too many regens: raise `hysteresis_min` slightly or tighten `max_regen`; if too few interventions on bad turns, lower thresholds or enable `use_calibrated_risk` with reviewed weights.
3. Enable **`HUMAN_METACOG_LOGPROBS`** only on providers that return logprobs; compare `logprob_mean` in `metacog_history` with `risk_score` / `outcome_proxy`.
4. Compare **`hula_tool_turns`** to total tool-heavy turns (proxy: logs or custom metrics) to see HuLa adoption vs dispatcher fallback.

## CI and cross-platform parity

GitHub Actions runs Linux and macOS builds plus the full C test suite and UI checks. **Before pushing:**

```bash
bash scripts/verify-all.sh
```

If CI fails on one OS only: reproduce with the same preset as CI (`cmake --preset test` or see `.github/workflows/ci.yml`), then fix **warnings-as-errors** and **feature macros** (`HU_IS_TEST`, channel flags) first — those differ most between matrix jobs.

## Related code

- Metacognition core: `src/cognition/metacognition.c`, `include/human/cognition/metacognition.h`
- Turn integration: `src/agent/agent_turn.c`
- Named spawn policy inheritance: `hu_spawn_config_apply_current_tool_agent` in `src/agent/spawn.c`
- Analytics script: `scripts/metacog-analytics.sh`
