---
title: "Skills vs agents — when to use which"
created: 2026-03-20
status: active
---

# Skills vs agents — when to use which

## Skills (SkillForge)

- **What:** Installed manifests (`*.skill.json`) + optional `SKILL.md`; surfaced in the system prompt as **Available Skills** (catalog) and loaded on demand via **`skill_run`**.
- **Use for:** Stable playbooks, tone/policy (e.g. `twin-*` skills), repeatable procedures, progressive disclosure (keep prompts small).
- **Dynamic catalog:** Set `HUMAN_SKILLS_CONTEXT=top_k` to inject only the top keyword-matched skills plus a footer; tune with `HUMAN_SKILLS_TOP_K` (default 12) and `HUMAN_SKILLS_CONTEXT_MAX_BYTES` (default 8192).

## Agents (pool spawn)

- **What:** **`agent_spawn`** / **`/spawn`** runs a **separate** `hu_agent_turn` with its own task string.
- **Use for:** Parallel work, long sub-tasks, or isolation; **child agents inherit** parent tools, memory, session store, observer, policy, autonomy level, and SkillForge when spawned from the main agent (so `skill_run` and the skills catalog stay available).

## Together

- Prefer **skills** for constraints and **agents** for execution surface area.
- Do not rely on a spawned agent if the parent did not pass tools/memory (e.g. custom embedders that omit the pool); behavior is documented in the unification plan.

## References

- [`docs/plans/2026-03-20-static-skills-dynamic-agents-unification.md`](../../plans/2026-03-20-static-skills-dynamic-agents-unification.md)
- [`human-skills/REGISTRY.md`](../../../human-skills/REGISTRY.md)
