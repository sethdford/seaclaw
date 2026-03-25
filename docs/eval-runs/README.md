---
title: Evaluation run records
description: How to capture and file summaries from live eval fleet runs
---

# Evaluation run records

Use this folder for **human-written** summaries of live fleet runs (never commit API keys or raw `.env`).

## How to produce a run

1. Build: `cmake --build build` (or `cmake --preset dev && cmake --build --preset dev`).
2. Load secrets in your shell only (e.g. `set -a && source .env && set +a`).
3. Run the fleet:
   - `REDTEAM_FLEET_LIVE=1 bash scripts/redteam-eval-fleet.sh`
   - **OpenAI judge, Gemini (or other) agent:** set `REDTEAM_HARNESS_USE_OPENAI=1` and keep `HUMAN_PROVIDER` / `HUMAN_MODEL` for the agent in `.env`. Do **not** set `REDTEAM_AGENT_USE_OPENAI` unless you want the agent on OpenAI too.
   - **Pin agent explicitly:** `REDTEAM_AGENT_USE_OPENAI=1` or `REDTEAM_AGENT_USE_GEMINI=1` (see `scripts/redteam-eval-fleet.sh` header).
4. Artifacts land under `build/redteam-fleet-reports/live-<timestamp>/`.

### Harness vs static `human eval run`

The Python harness (`scripts/adversarial-eval-harness.py`) passes each suite task’s **`expected`** and **`rubric`** text into the LLM judge so scores align with `eval_suites/*.json`. If multi-turn or adversarial harness results looked worse than static evals before that wiring, rerun the live harness after updating the script. Notes: [2026-03-21 multi-turn / adversarial triage](2026-03-21-multi-turn-adversarial-triage.md).

## What to copy into a summary

Copy [`docs/templates/eval-release-notes.md`](../templates/eval-release-notes.md) and fill:

- `HUMAN_PROVIDER`, `HUMAN_MODEL`, `ADV_EVAL_MODEL` (judge), suite manifest date from `eval_suites/MANIFEST.md`.
- Tables from each `eval-*.log` final JSON line and `harness-report.json` `summary` + failed probes.

## Naming

Prefer `YYYY-MM-DD-<short-label>.md` (e.g. `2026-03-22-gemini-agent-openai-judge.md`) and keep one line in this README linking to the latest file when you add it.

## Latest recorded run

- [2026-03-22 — OpenAI agent gpt-4o-mini, local `.env`](2026-03-22-fleet-summary.md) (harness mean **0.935**, static `reasoning_basic` **100%**).

## In-repo validation notes (no API required to read)

- [2026-03-22 — Compaction, harness hardening, `agent -m` exit codes](2026-03-22-validation-and-cli-exit.md) — runbook after the validation batch; live fleet still requires your keys locally.

## HuLa execute + trace (CLI)

- `human hula run <file-or-json>` prints an execution trace to stdout.
- Set **`HU_HULA_TRACE_DIR`** to a writable directory to also persist a JSON trace file (`hu_hula_trace_persist`). Quick check: `bash scripts/hula-smoke.sh` (uses a temp dir by default).
- Example program: `examples/hula_research_pipeline.json` (demo tools `search` / `analyze` / `write` — same as `human hula run` help text).
