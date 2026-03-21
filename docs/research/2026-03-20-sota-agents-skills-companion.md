---
title: "SOTA research map — agents, skills, companions, evaluation"
created: 2026-03-20
status: living
audience: maintainers, product, research
---

# SOTA research map — agents, skills, companions, evaluation

This note **synthesizes** directions from **labs, papers, and production engineering writeups** (not an implementation checklist). Use it to steer **h-uman** toward defensible SOTA: *what the field agrees on*, *what is still contested*, and *what maps to this codebase*.

---

## 1. Multi-agent systems (orchestration & economics)

### Anthropic — multi-agent research system (engineering, 2025)

- **Idea:** Hierarchical **lead + parallel subagents**; lead plans and persists plans to avoid truncation; subagents explore sub-questions; results merged with citations.
- **Empirical claim:** Large internal gains vs single-agent on research-style tasks; **token usage ~15×** typical chat — multi-agent is a **cost/quality trade**, not a default.
- **Reference:** [How we built our multi-agent research system](https://www.anthropic.com/engineering/multi-agent-research-system) (Anthropic Engineering).

**Implications for h-uman**

- Your **orchestrator + spawn** path aligns with “parallel breadth” patterns; **missing piece for SOTA** is explicit **budgeting** (token/cost ceilings per spawn), **plan persistence** (SQLite or session artifact), and **merge policies** (citation/consensus — you already have consensus-style paths in code; keep them **eval-driven**).
- Treat **nested spawn** as a **policy-gated** feature (depth, max concurrent, spend).

### OpenAI — Agents SDK (handoffs vs agents-as-tools)

- **Patterns:** (1) **Handoffs** — specialist becomes active; (2) **Agents as tools** — manager retains control and composes outputs.
- **Reference:** [Multi-agent / handoffs](https://openai.github.io/openai-agents-python/ref/handoffs/) (OpenAI Agents SDK docs); [Cookbook: orchestrating agents](https://developers.openai.com/cookbook/examples/orchestrating_agents/).

**Implications for h-uman**

- **`delegate`** / mailbox vs **`agent_spawn`** mirror this split. SOTA clarity is **naming + docs**: when to hand off conversation vs spawn a **bounded worker** that returns a result.
- **Handoffs as tools** (`transfer_to_*`) is a **UX/schema** pattern worth mirroring in **tool descriptions** so models route reliably.

---

## 2. Skills, tools, and retrieval (scaling the catalog)

### Research — tool/agent retrieval in shared space

- **Tool-to-Agent Retrieval** — embed **tools and parent agents** in a shared vector space with relational metadata; improves recall on **LiveMCPBench**-style settings.
- **Reference:** [arXiv:2511.01854](https://arxiv.org/abs/2511.01854) — *Tool-to-Agent Retrieval: Bridging Tools and Agents for Scalable LLM Multi-Agent Systems*.

- **Agent-as-a-Graph** — graph + vector + **weighted RRF** reranking.
- **Reference:** [arXiv:2511.18194](https://arxiv.org/abs/2511.18194) (as indexed in search summaries; verify exact title on arXiv before citing externally).

### Production survey — selection layers

- Surveys distinguish **UI/manual** vs **retrieval-based** vs **autonomous** tool/agent selection and stress **production** failure modes (staleness, permission boundaries, observability).
- **Reference:** Sciety / preprint ecosystem — search for *“Tool and Agent Selection for Large Language Model Agents in Production: A Survey”* (DOI preprints202512.1050.v1 per indexers; verify final venue).

**Implications for h-uman**

- Your **`HUMAN_SKILLS_CONTEXT=top_k`** keyword router is a **cheap** first step; SOTA adjacent step is **embedding retrieval** over `(skill name, description, tags, SKILL.md chunk)` with **filters** (channel, persona, policy tier).
- **Tool-to-agent** insight: retrieve **both** “which skill” and “whether to spawn a specialist agent” in **one** ranked list to avoid siloed pipelines.

---

## 3. Memory & long-horizon agents

### MemGPT — virtual context management (Berkeley, 2023)

- **Idea:** OS-inspired **tiers** (fast context vs archival); explicit **control flow** to move data between tiers; “unbounded” effective context via **eviction/paging** policies.
- **Reference:** [MemGPT: Towards LLMs as Operating Systems](https://arxiv.org/abs/2310.08560); [Sky Computing lab page](https://sky.cs.berkeley.edu/project/memgpt/).

**Implications for h-uman**

- You already have **memory engines, tiers, compaction** — SOTA alignment is **documenting the paging policy** (what promotes to core prompt, what stays recall-only, what is never auto-injected) and **measuring** regression on **long-session** evals.

---

## 4. Embodied / generalist “skills” (different but instructive)

### DeepMind — SIMA / SIMA 2

- **Idea:** **Large skill libraries** in embodied settings; SIMA 2 adds **reasoning**, **dialogue**, and **self-improvement** in new environments (lab + tech reports).
- **References:** SIMA 1 [arXiv:2404.10179](https://arxiv.org/abs/2404.10179); SIMA 2 [arXiv:2512.04797](https://arxiv.org/abs/2512.04797); [DeepMind blog](https://deepmind.com/blog/sima-2-an-agent-that-plays-reasons-and-learns-with-you-in-3d-virtual-worlds/).

**Implications for h-uman**

- Not 1:1 (you are not a game agent), but the **lesson** is: **skills = composable primitives + curriculum + evaluation in environment**. For you, “environment” is **channels + tools + policy sandbox**.

---

## 5. Digital twins & personalized companions (ACL / arXiv line)

Recent work separates **fidelity** (does it sound like the user?) from **alignment** (does it act in their interest?) and **temporal** consistency.

| Work (indicative) | Focus |
| ------------------ | ----- |
| **PersonaTwin** | Multi-tier prompt conditioning; healthcare-scale evaluation — [arXiv:2508.10906](https://arxiv.org/abs/2508.10906) / [ACL Anthology](https://aclanthology.org/2025.gem-1.66/) |
| **TwinVoice** | Benchmark: social vs interpersonal vs narrative persona + multi-capability scoring — [arXiv:2510.25536](https://arxiv.org/abs/2510.25536) |
| **DPRF** | Dynamic persona refinement from behavioral divergence — [arXiv:2510.14205](https://arxiv.org/abs/2510.14205) |
| **TWICE** | Long-horizon user simulation (e.g. social posts) with memory + style — [arXiv:2602.22222](https://arxiv.org/abs/2602.22222) |

**Implications for h-uman**

- Your **`twin-*` skills** are the **right layer** for *stated* policies; SOTA requires **paired evals** (TwinVoice-style dimensions) and optional **refinement loops** (DPRF-style) — *not* only longer prompts.
- **Temporal** behavior (TWICE) argues for **event-stamped memory** and **explicit “why now”** in proactive features — ties to your feeds/awareness direction.

---

## 6. Evaluation (what “SOTA” must prove)

Production SOTA is increasingly **eval-first**, not model-first.

### τ-bench (tool–agent–user)

- **Idea:** Multi-turn **simulated user** + **domain tools** + **policy** (airline, retail; extensions e.g. telecom in τ²-bench); **Pass^k** metrics for reliability across attempts.
- **References:** [arXiv:2406.12045](https://arxiv.org/abs/2406.12045) — *τ-bench: A Benchmark for Tool-Agent-User Interaction in Real-World Domains*; implementation [sierra-research/tau-bench](https://github.com/sierra-research/tau-bench).

- **Multi-agent:** measure **task success**, **citation accuracy**, **cost**, **latency**, **failure modes** (loops, contradictory merges).
- **Tool/skill routing:** **Recall@k / nDCG** on query→tool/skill datasets (cf. LiveMCPBench-style benchmarks cited in retrieval papers above).
- **Digital twin:** **lexical + persona + consistency** suites (TwinVoice-style); guard against **overfitting** to shallow style.

**Implications for h-uman**

- Extend **`human eval`** / golden suites with **small, versioned** scenarios: spawn inheritance, skill catalog truncation, boundary-guard conflicts, multi-agent merge.
- Log **which skills** were in context (`top_k` vs all) for **replay**.

---

## 7. Prioritized roadmap (honest “closer to SOTA”)

| Priority | Item | Research lineage |
| -------- | ---- | ---------------- |
| P0 | **Budgets** on spawn (tokens/$/depth) + metrics | Anthropic token economics |
| P1 | **Embedding retrieval** for skills/tools + optional graph edges (deps, “pairs with”) | Tool-to-Agent Retrieval; Agent-as-a-Graph |
| P2 | **Eval dimensions** for twin fidelity + safety (mini TwinVoice / PersonaTwin-style) | §5 papers |
| P3 | **Handoff tools** with explicit schemas (`transfer_to_*`) | OpenAI Agents SDK pattern |
| P4 | **Plan persistence** for orchestrator (durable artifact per run) | Anthropic lead-agent pattern |
| P5 | **Persona refinement loop** (opt-in, privacy-preserving) | DPRF-style |

---

## 8. Cross-links in this repo

- Runtime merge of skills + agents: [`docs/standards/ai/skills-vs-agents.md`](../standards/ai/skills-vs-agents.md)
- Implementation history: [`docs/plans/2026-03-20-static-skills-dynamic-agents-unification.md`](../plans/2026-03-20-static-skills-dynamic-agents-unification.md)
- Skill registry (human-facing): [`human-skills/REGISTRY.md`](../../human-skills/REGISTRY.md)

---

## Disclaimer

Links and arXiv IDs are **starting points**; venues and versions change. Before external publication, **open each primary source** and cite the canonical PDF/DOI.
