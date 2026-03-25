---
title: Validation batch — compaction, harness, CLI exit
description: What shipped in-repo and how to confirm E2E locally
---

# Validation batch (2026-03-22)

## Code changes to validate

| Area | Change |
|------|--------|
| **Compaction** | `hu_context_compact_for_pressure` no longer leaves orphan `tool` rows after removing an assistant `tool_calls` message (fixes OpenAI `tool_call_id` / `tool_calls` HTTP 400). Tests: `test_context_compact_pressure_swallows_tools_after_assistant` in suite **Agent subsystems**. |
| **Fleet script** | Safe empty `HARNESS_PY_ARGS` under `set -u`; default agent subprocess timeout **300s**; optional `ADV_EVAL_TIMEOUT_SEC`. |
| **Harness** | Judge/generator HTTP timeouts: default **180s**, **3×** retry on read timeout; optional `ADV_EVAL_TIMEOUT_SEC`; subprocess timeout returns probe failure instead of crashing. |
| **CLI** | `human agent -m "…"` / `--message` propagates the turn’s `hu_error_t` (non-zero on failure). |
| **Degradation** | `hu_provider_degrade_chat` returns **`HU_ERR_PROVIDER_RESPONSE`** for circuit-open and exhausted-retry honest-failure messages (response text still filled). `hu_agent_turn` copies that text to the user, appends assistant history, and returns **`HU_ERR_PROVIDER_RESPONSE`** so the CLI exits non-zero while still printing the line. |
| **Smoke / local HOME** | If you still see OpenAI `tool_call_id` / `tool_calls` **400** on `agent -m`, stale session history under **`~/.human/`** may predate the compaction fix—use an isolated `HOME` for a clean run (as the fleet does) or start a fresh session. |

## Commands (local E2E)

```bash
cmake --build build -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
./build/human_tests --suite="Agent subsystems"
./build/human doctor
# With API keys loaded (do not commit .env):
set -a && source .env && set +a
./build/human agent -m "Reply with exactly: pong"
echo "agent -m exit: $?"
export REDTEAM_FLEET_LIVE=1 REDTEAM_FLEET_AGENT_SMOKE=1
export ADV_EVAL_TIMEOUT_SEC=240   # optional
bash scripts/redteam-eval-fleet.sh
```

Artifacts from live fleet: `build/redteam-fleet-reports/live-<UTC>/` (`eval-*.log`, `harness-report.json`, `harness-console.log`).

## Product / process pointer

For ongoing “reliable assistant” workstreams, see [`docs/plans/2026-03-22-reliable-accountable-platform.md`](../plans/2026-03-22-reliable-accountable-platform.md).
