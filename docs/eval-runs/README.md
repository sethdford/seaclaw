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
   - Optional: `REDTEAM_HARNESS_USE_OPENAI=1` if the judge should use OpenAI while the agent uses Gemini (see comments in `scripts/redteam-eval-fleet.sh`).
4. Artifacts land under `build/redteam-fleet-reports/live-<timestamp>/`.

## What to copy into a summary

Copy [`docs/templates/eval-release-notes.md`](../templates/eval-release-notes.md) and fill:

- `HUMAN_PROVIDER`, `HUMAN_MODEL`, `ADV_EVAL_MODEL` (judge), suite manifest date from `eval_suites/MANIFEST.md`.
- Tables from each `eval-*.log` final JSON line and `harness-report.json` `summary` + failed probes.

## Naming

Prefer `YYYY-MM-DD-<short-label>.md` (e.g. `2026-03-22-gemini-agent-openai-judge.md`) and keep one line in this README linking to the latest file when you add it.
