---
title: Multi-turn & adversarial harness triage
description: Why harness scores could trail static eval and what changed
---

# Multi-turn and adversarial harness triage (2026-03-21)

## Symptom

Live **`adversarial-eval-harness.py`** runs sometimes showed weaker **multi_turn** / **adversarial** pass rates than **`human eval run`** on the same JSON suites.

## Root cause (fixed)

The harness LLM judge received only a **generic profile** (safety, human_likeness, tool_capability, etc.) plus the probe text and the assistant transcript. It did **not** receive each task’s suite-level **`expected`** and **`rubric`** strings from `eval_suites/multi_turn.json` and `eval_suites/adversarial.json`.

Static `human eval run` merges those fields into the judge prompt; the harness did not, so calibration drifted and borderline replies were scored inconsistently.

## Fix

`scripts/adversarial-eval-harness.py` now appends **`expected`** / **`rubric`** to the judge user message when present on a probe (including multi-turn tasks loaded from suite JSON).

## Follow-up

After pulling this change, re-run a live fleet and compare `harness-report.json` to prior baselines. Remaining gaps are more likely **true model behavior** (e.g. **mt-003** escalating boundary, **adv-005** encoding smuggle) rather than judge–suite mismatch.
