---
title: "Static skills + dynamic agents — unified behavior"
created: 2026-03-20
status: planned
scope: skillforge, prompt injection, skill_run, agent_spawn, orchestrator, inheritance, retrieval, tests
---

# Plan: Static skills + dynamic agents — unified behavior

**Goal:** Make **registry / installed skills** (static playbooks + optional commands) and **dynamic agents** (pool spawn, orchestrator, sub-agents, delegation) **compose predictably**: same policies, no silent contradictions, measurable quality.

**Non-goal:** Replace either layer with the other; both remain first-class.

---

## 1. Current architecture (as implemented)

| Layer | What it does | Primary code paths |
|--------|----------------|-------------------|
| **Skill discovery** | Loads `~/.human/skills/` (and similar) into `hu_skillforge_t` at bootstrap | `src/bootstrap.c` (`hu_skillforge_discover`), `src/skillforge.c` |
| **Static skill context in prompt** | Injects **name + description** for every **enabled** skill under `## Available Skills` | `src/agent/agent_turn.c` → `hu_prompt_build_system` in `src/agent/prompt.c` (after Intelligence block, before Memory Context) |
| **Contact-scoped skills** | SQLite-backed learned / contact skills merged into the same skills string | `src/agent/agent_turn.c`, `hu_skill_build_contact_context` (`include/human/intelligence/skills_context.h`) |
| **Progressive disclosure** | Full **SKILL.md** loaded when the model calls **`skill_run`** (and optional shell command under policy) | `src/tools/skill_run.c` |
| **Dynamic agents** | **`agent_spawn`** tool → `hu_agent_pool_spawn`; CLI **`/spawn`**; orchestrator decomposition + registry agents | `src/tools/agent_spawn.c`, `src/agent/spawn.c`, `src/agent/agent_turn.c` (orchestrator), `src/agent/commands.c` |
| **Delegation** | **`delegate`** tool for cross-agent handoff | `src/tools/delegate.c` |

**Implication:** Today, “static skills” are **mostly a catalog + short descriptions** in the system prompt; **deep instructions** arrive via **`skill_run`**. Dynamic agents are a **separate execution path** with their own config (e.g. persona copied from parent in `agent_spawn`).

---

## 2. Gaps that prevent “flawless” integration

1. **No semantic skill selection** — All enabled skills are listed; long registries inflate prompts and dilute attention. *Dynamic* skill routing (retrieve top-k by query / tags / embeddings) is not wired into `agent_turn.c`.
2. **Spawned agent parity** — Child agents need a **defined contract**: same `skillforge`? Same twin / boundary skills? Same autonomy and sandbox? **Audit** `hu_agent_pool_spawn` + worker thread initialization vs main agent.
3. **Priority / conflict rules** — e.g. `twin-boundary-guard` vs an spawned agent instructed to “say yes to everything” is undefined. Need **explicit precedence**: persona > twin static skills > task-specific spawn instructions > generic helpfulness.
4. **Orchestrator ↔ skills** — Multi-agent decomposition does not formally **assign** a skill pack per sub-task (e.g. “use `twin-meeting-wingman` for step 2”). Optional enhancement: task labels → suggested `skill_run` or spawn preset.
5. **Observability** — Hard to answer: “Which skills were in context?” “Did a child agent run without boundary skills?” Need **structured logs / metrics** (behind existing observability hooks), not only stderr.
6. **Tests** — Few (or no) **integration** tests that combine `skill_run` + `agent_spawn` in one scenario with assertions on policy and prompt sections.

---

## 3. Target model (conceptual)

```
┌─────────────────────────────────────────────────────────────┐
│  Layer A — Policy & persona (stable, auditable)              │
│  Persona, twin-* skills (short + full via skill_run), prefs  │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Layer B — Context assembly (may become retrieval-based)     │
│  Top-k skills + memory + STM + intelligence block            │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Layer C — Dynamic execution                                 │
│  Tools, skill_run, agent_spawn, orchestrator, delegate       │
└─────────────────────────────────────────────────────────────┘
```

**Rules:**

- **Layer A** always applies to **root** agent; **child agents** must either inherit Layer A or **explicitly** document what was dropped (for debugging).
- **Layer C** must not bypass **security policy** or **autonomy** gates already enforced for the parent unless the spawn API documents an override and tests cover it.

---

## 4. Phased implementation plan

### Phase 0 — Inventory & contracts (docs + checklist)

- [ ] Document in **`docs/CONCEPT_INDEX.md`** (or `ARCHITECTURE.md`): skillforge lifecycle, prompt section order, spawn inheritance.
- [ ] Add **`docs/standards/ai/`** short note: “Skills vs agents — when to use which” (link this plan).
- [ ] **Spawn inheritance matrix** (table): `skillforge`, persona, policy, tools whitelist, memory — row per field, column parent vs child.

**Exit:** Maintainer can answer inheritance questions without reading C for an hour.

### Phase 1 — Parity & safety (code)

- [ ] **Audit** child agent init: ensure `skillforge` pointer (or equivalent capability list) is passed or intentionally `NULL` with user-visible behavior.
- [ ] If children **lack** skillforge, inject a **minimal static line** in child system prompt: “Parent skills not loaded; use `delegate` or return to parent for skill-backed workflows” — or pass skillforge (preferred for parity).
- [ ] **Precedence helper** (optional small module): single function that orders “constraints” for conflict resolution; unit tests with canned strings.

**Exit:** No silent downgrade of twin/boundary behavior without an explicit code path and test.

### Phase 2 — Dynamic skill context (retrieval / routing)

- [ ] **Config flag** (YAGNI-safe): e.g. `skills.context_mode = all | top_k` with `top_k` default off until embeddings + memory path exist.
- [ ] Implement **top-k** using existing memory / embedding stack if available; fallback keyword match on skill name + tags from `.skill.json` / registry.
- [ ] Cap **total skill context bytes**; overflow → “N more skills available; use skill_run by name.”

**Exit:** Large registries (60+ skills) do not linearly blow prompt size when mode is `top_k`.

### Phase 3 — Orchestrator integration (optional, medium lift)

- [ ] Extend decomposition output (or follow-up prompt) to attach **suggested_skill** per task (string name, optional).
- [ ] Telemetry: log suggested vs actually invoked `skill_run`.

**Exit:** Multi-step goals can be traced to skill usage per sub-task.

### Phase 4 — Evaluation & CI

- [ ] **Scenario tests** (deterministic, `HU_IS_TEST`): e.g. mock provider asserts system prompt contains `## Available Skills` and contains `twin-boundary-guard` when enabled.
- [ ] **Spawn + skill** test: parent enables a skill; spawn child; assert child prompt or tool surface matches contract from Phase 0 matrix.
- [ ] Add **`scripts/validate-skill-registry.sh`** already in `verify-all`; extend with optional **“skill names referenced in docs match registry”** grep (low priority).

**Exit:** Regressions in integration are caught in CI, not in production.

### Phase 5 — Product UX

- [ ] CLI / gateway: **`human skills list --active`** and **`human agent spawn --help`** cross-link to “Skills vs agents” doc.
- [ ] Dashboard (optional): show active skills count + last `skill_run` / spawn (from metrics).

---

## 5. Acceptance criteria (“flawless together”)

1. **Single session**: User with `twin-boundary-guard` + `twin-identity-sync` enabled gets **consistent** behavior whether the model answers inline or calls **`agent_spawn`** (within documented inheritance).
2. **Explicit degradation**: If child agents cannot load skills, the product **states** that (log + optional user-visible note), never silent.
3. **Token safety**: With 60+ registry skills, enabling `top_k` mode keeps prompt under configured budget while still allowing **`skill_run`** for any installed name.
4. **CI**: At least **two** new tests covering skill prompt injection + spawn parity (or documented skip with issue link).
5. **Docs**: This plan linked from **`human-skills/REGISTRY.md`** and architecture index.

---

## 6. Suggested owners & sequencing

| Phase | Rough effort | Risk |
|-------|----------------|------|
| 0 | Low | None |
| 1 | Medium | High (spawn path) |
| 2 | Medium–high | Medium (retrieval quality) |
| 3 | Medium | Low |
| 4 | Medium | Low |
| 5 | Low | Low |

**Recommended order:** 0 → 1 → 4 (minimal tests for 1) → 2 → 5 → 3.

---

## 7. References

- `src/agent/agent_turn.c` — `skills_ctx`, orchestrator block
- `src/agent/prompt.c` — `## Available Skills`, ordering vs memory
- `src/tools/skill_run.c` — full SKILL.md load
- `src/tools/agent_spawn.c`, `src/agent/spawn.c` — dynamic agents
- `human-skills/REGISTRY.md`, `skill-registry/registry.json`
- `scripts/validate-skill-registry.sh`

---

*Created: 2026-03-20. Update this plan when Phase 1 audit completes (fill inheritance matrix with facts).*
