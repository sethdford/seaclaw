# Eval Suites Manifest

Version: **2026-03-22b** (bump on any task add/remove/reword or judge profile change)

## Suites

| File | Tasks | Domain | Judge Profiles | Version |
| ---- | ----: | ------ | -------------- | ------- |
| `adversarial.json` | 10 | Safety red-team | `safety` | 1.0 |
| `capability_edges.json` | 10 | Epistemic honesty / no-AGI | `capability_honesty` | 1.0 |
| `coding_basic.json` | 5 | Code generation | (contains) | 1.0 |
| `fidelity.json` | 10 | Persona fidelity | (contains) | 1.0 |
| `hula_orchestration.json` | 2 | HuLa IR: par gather + branch | `hula_structure` (harness); `llm_judge` (static) | 1.0 |
| `human_likeness.json` | 8 | Tone / warmth / register | `human_likeness` | 1.0 |
| `intelligence.json` | 10 | Reasoning + knowledge | (contains) | 1.0 |
| `memory.json` | 8 | Memory ops | (contains) | 1.0 |
| `multi_turn.json` | 6 | Multi-turn conversation arcs | mixed | 1.0 |
| `reasoning.json` | 10 | Reasoning depth | (contains) | 1.0 |
| `reasoning_basic.json` | 10 | Basic reasoning | `llm_judge` | 1.1 |
| `social.json` | 8 | Social intelligence | (contains) | 1.0 |
| `tool_capability.json` | 8 | Tool discipline / no fabrication | `tool_capability` | 1.0 |
| `tool_use.json` | 8 | Tool selection | (contains) | 1.0 |
| `tool_use_basic.json` | 5 | Basic tool selection | `llm_judge` | 1.0 |

**Total**: 15 suites, 128 tasks

Human-facing HuLa documentation (config, CLI, ethics, traces): [`docs/guides/hula.md`](../docs/guides/hula.md).

## Rules

1. **Bump version** when adding, removing, or rewording a task, changing `expected`/`rubric`, or modifying a judge profile.
2. **New ids** must be globally unique across all suites (enforced by `test_eval_expanded_suite_json_files_parse_unique_ids_expected_counts`).
3. **Holdout discipline**: if you tune prompts against a suite, mark that suite as "training" in your claim; use other suites as held-out evidence.
4. **Judge model**: pin in your claim (see `docs/standards/ai/capability-claims.md`). Default harness judge: `gpt-4o-mini` via `ADV_EVAL_MODEL`.

## Changelog

- **2026-03-22b**: Added `hula_orchestration.json` (tasks `hula-001`, `hula-002`) and harness judge profile `hula_structure` for HuLa-shaped JSON plans.
- **2026-03-22**: `reasoning_basic.json` now uses `llm_judge` with per-task rubrics; `human eval run` passes rubric + gold reference to the judge when both are present.
- **2026-03-21**: Initial manifest. Added `human_likeness.json`, `tool_capability.json`, `multi_turn.json`. Harness supports multi-turn scenarios.
