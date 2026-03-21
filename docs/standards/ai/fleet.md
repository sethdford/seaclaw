---
title: "Fleet — pooled sub-agents (spawn) limits"
created: 2026-03-20
status: active
---

# Fleet — pooled sub-agents (spawn) limits

The **fleet** is the **`hu_agent_pool_t`** that backs **`agent_spawn`**, **`/spawn`**, **`delegate`** (named agents), and related slash commands. It is separate from the **scheduler** `max_concurrent` used by `hu_subagent_manager_t`.

## Limits (config → pool)

Set under `agent` in `~/.human/config.json`:

| Key | Default | Meaning |
| --- | ------- | ------- |
| `pool_max_concurrent` | 8 | Max **running** one-shot/persistent workers at once |
| `fleet_max_spawn_depth` | 8 | Max **nesting** depth (child depth = parent `spawn_depth` + 1). `0` = unlimited |
| `fleet_max_total_spawns` | 0 | Lifetime **starts** in this process (`0` = unlimited) |
| `fleet_budget_usd` | 0 | Session spend cap (`0` = off). Requires a **bound** or **inherited** `hu_cost_tracker_t` |

## Runtime behavior

- **Root** agents have `spawn_depth == 0`. Each successful spawn creates a child with `spawn_depth == parent + 1`.
- **`shared_cost_tracker`**: Spawn paths pass the parent’s cost tracker when available so token usage accrues to the same session and **fleet budget** checks use `hu_cost_session_total`.
- **`hu_agent_pool_bind_fleet_cost_tracker`**: Optional pool-level tracker for budget checks when the caller does not pass one, and for **`/fleet`** “session spend” when no per-spawn tracker is set.

## Observability

- **`/fleet`** — limits, running count, slots in use, lifetime spawns started, session spend (when a pool cost tracker is bound).
- **`/agents`** — per-slot status (unchanged).

## Errors

| Code | When |
| ---- | ---- |
| `HU_ERR_FLEET_DEPTH_EXCEEDED` | Would exceed `fleet_max_spawn_depth` |
| `HU_ERR_FLEET_SPAWN_CAP` | Would exceed `fleet_max_total_spawns` |
| `HU_ERR_FLEET_BUDGET_EXCEEDED` | Session total already ≥ `fleet_budget_usd` |
| `HU_ERR_INVALID_ARGUMENT` | `fleet_budget_usd > 0` but no cost tracker available for the check |

## References

- [`docs/standards/ai/skills-vs-agents.md`](skills-vs-agents.md) — when to spawn vs use skills
- [`docs/research/2026-03-20-sota-agents-skills-companion.md`](../../research/2026-03-20-sota-agents-skills-companion.md) — multi-agent economics
