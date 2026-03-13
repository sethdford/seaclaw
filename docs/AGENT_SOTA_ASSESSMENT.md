# human C11 AI Runtime — SOTA Autonomous Agent Assessment

**Assessment date:** 2025-03-13  
**Scope:** 10 capability areas, rating 1–5 (5 = state-of-the-art)

---

## Executive Summary

The human runtime is a **mature, production-grade** autonomous agent system with strong foundations in memory, tools, security, and persona. It excels at relationship-aware AI, emotional intelligence, and pragmatic tool execution. Gaps exist primarily in **advanced reasoning** (ToT, MCTS, beam search), **multi-agent orchestration** (debate, consensus), **alignment tooling** (constitutional AI, RLHF), and **LLMCompiler integration** (implemented but not wired into the main loop).

---

## 1. Reasoning & Planning

**Rating: 3/5**

### EXISTS

| Capability                          | Evidence                                                                                                           |
| ----------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| **LLM-based plan generation**       | `planner.c:155–251` — `hu_planner_generate` builds system prompt with tool names, calls provider, parses JSON plan |
| **Failure-driven replanning**       | `planner.c:261–394` — `hu_planner_replan` with goal + progress + failure detail                                    |
| **Linear plan execution**           | `planner.c:117–126` — `hu_planner_next_step` returns next pending step                                             |
| **Chain-of-thought (prompt-level)** | `prompt.c:307–328` — injects reasoning instructions when `chain_of_thought` is true                                |
| **Reflection-guided retry**         | `agent_turn.c:702–731` — `hu_reflection_evaluate` / `hu_reflection_evaluate_llm`, critique prompt, retry loop      |
| **A/B response evaluation**         | `agent_turn.c:734–818` — generates up to 3 candidates, picks best via `hu_ab_evaluate`                             |

### MISSING

- **Tree-of-thought (ToT)**: No branching exploration
- **MCTS / beam search**: No search over reasoning paths
- **Explicit multi-step reasoning traces**: CoT is instruction injection, not structured trace
- **Structured planning with DAG**: `llm_compiler.c` exists but is **not wired** into `agent_turn.c` (see `docs/plans/2026-03-08-better-than-human.md`)

### Highest-Impact Missing

**Tree-of-thought or beam search** — would enable exploration of alternative reasoning paths before committing to tool calls, improving robustness on complex multi-step tasks.

---

## 2. Memory Architecture

**Rating: 4/5**

### EXISTS

| Capability                | Evidence                                                                                                   |
| ------------------------- | ---------------------------------------------------------------------------------------------------------- |
| **Episodic memory**       | `episodic.c` — SQLite-backed episodes with impact, salience, emotional arc, key moments                    |
| **Temporal indexing**     | `retrieval/temporal.c` — `hu_temporal_decay_score` for recency-weighted retrieval                          |
| **Semantic search / RAG** | `retrieval/engine.c`, `retrieval/hybrid.c`, `rag_pipeline.c` — keyword + semantic + hybrid, temporal decay |
| **Memory consolidation**  | `consolidation.c`, `consolidation_engine.c` — merge/dedupe via similarity                                  |
| **Forgetting curves**     | `forgetting_curve.c`, `forgetting.c` — Ebbinghaus-style decay, emotional anchors                           |
| **Working memory (STM)**  | `stm.c` — `hu_stm_buffer_t` with turn slots, entities, emotions, primary topic                             |
| **Knowledge graph**       | `graph.c` — entities, relations, mention counts, temporal metadata                                         |
| **Life chapters**         | `life_chapters.c` — narrative summarization                                                                |
| **Deep memory**           | `deep_memory.c`, `deep_extract.c` — fact extraction, subject-predicate-object                              |

### MISSING

- **Hierarchical memory consolidation** (e.g., sleep-like replay)
- **Explicit memory importance prediction** (learned salience model)
- **Cross-session memory consolidation** beyond batch decay

### Highest-Impact Missing

**Learned importance prediction** — currently salience is heuristic; a learned model could predict which memories are worth retaining long-term.

---

## 3. Tool Use

**Rating: 4/5**

### EXISTS

| Capability                  | Evidence                                                                           |
| --------------------------- | ---------------------------------------------------------------------------------- |
| **67+ tools**               | `factory.c` — file, shell, web, memory, calendar, messaging, analytics, etc.       |
| **Parallel tool execution** | `dispatcher.c` — pthread-based parallel dispatch when `tc_count > 1`               |
| **Tool routing**            | `tool_router.c` — keyword-based relevance scoring for tool subset selection        |
| **DAG executor**            | `dag_executor.c` — `hu_dag_next_batch`, `hu_dag_resolve_vars` for `$tN` references |
| **LLMCompiler**             | `llm_compiler.c` — compiles natural language to DAG with deps                      |
| **Policy engine**           | `policy_engine.c` — allow/deny/require approval per tool + args                    |
| **Approval flow**           | `agent_turn.c:951–991` — `needs_approval` → callback, retry                        |

### MISSING

- **Tool chaining** (output of one tool as input to another) — DAG supports `$tN` but not wired into agent loop
- **Tool learning** (discovering new tools from usage)
- **Dynamic tool creation** (user-defined tools)

### Highest-Impact Missing

**LLMCompiler integration** — `llm_compiler_enabled` is in config but `agent_turn.c` always uses `hu_dispatcher_dispatch` directly. Wiring the compiler would enable DAG-based parallel execution with dependencies.

---

## 4. Multi-Agent

**Rating: 2/5**

### EXISTS

| Capability       | Evidence                                                                                       |
| ---------------- | ---------------------------------------------------------------------------------------------- |
| **Orchestrator** | `orchestrator.c` — task decomposition, agent registration, assign/complete/fail, merge results |
| **Mailbox**      | `mailbox.c` — inbox per agent, message queue                                                   |

### MISSING (in orchestrator)

- **LLM-based task decomposition** — `hu_orchestrator_propose_split` takes pre-split subtasks; no LLM call
- **Agent specialization** — agents registered with role/skills but no automatic routing
- **Consensus** — `hu_orchestrator_merge_results` is simple concatenation
- **Debate protocols** — no debate or voting
- **Collaborative planning** | `collab_planning.c` exists but is separate from orchestrator

### Highest-Impact Missing

**LLM-driven task decomposition** — the orchestrator currently accepts pre-split subtasks; no automatic goal → subtask decomposition.

---

## 5. Learning

**Rating: 3/5**

### EXISTS

| Capability           | Evidence                                                                           |
| -------------------- | ---------------------------------------------------------------------------------- |
| **Self-improvement** | `self_improve.c` — prompt patches from reflections, tool preferences               |
| **Online learning**  | `online_learning.c` — EMA-based strategy weights from tool success/failure         |
| **Value learning**   | `value_learning.c` — inferred values from corrections, approvals                   |
| **Meta-learning**    | `meta_learning.c` — confidence threshold, refinement weeks, discovery min feedback |
| **Reflection**       | `reflection.c` — quality evaluation, critique prompt, retry                        |
| **Skills**           | `skill_system.c`, `skills.c` — word-overlap matching, refinement, retirement       |

### MISSING

- **Curriculum learning** — no staged difficulty progression
- **Few-shot adaptation** — no dynamic few-shot example selection
- **Reward modeling** — no explicit reward signal from user feedback
- **RLHF** — no preference-based fine-tuning

### Highest-Impact Missing

**Reward modeling** — value learning from corrections is implicit; explicit reward signals (thumbs up/down, preference pairs) would enable better alignment.

---

## 6. Safety & Alignment

**Rating: 3/5**

### EXISTS

| Capability            | Evidence                                                                   |
| --------------------- | -------------------------------------------------------------------------- |
| **Policy engine**     | `policy_engine.c` — tool/args matching, deny/require approval              |
| **Input guards**      | `input_guard.c` — injection pattern detection (high/med risk)              |
| **Sandboxing**        | `sandbox.c` — Landlock, seccomp, bubblewrap, firejail, docker, firecracker |
| **Audit logging**     | `audit.c` — policy violations, command execution                           |
| **Autonomy levels**   | LOCKED, SUPERVISED, ASSISTED, AUTONOMOUS                                   |
| **Human-in-the-loop** | Approval callback for risky tools                                          |

### MISSING

- **Constitutional AI** — no explicit principles/rules in prompt
- **RLHF signals** — no preference-based fine-tuning
- **Uncertainty quantification** — standards mention uncertainty (e.g. `hallucination-prevention.md`) but no explicit confidence scores
- **Refusal calibration** — reflection detects "I cannot" / "I'm unable" but no calibration

### Highest-Impact Missing

**Constitutional AI** — explicit principles (e.g. "Be helpful, harmless, honest") in the system prompt would reduce harmful outputs.

---

## 7. Context Management

**Rating: 4/5**

### EXISTS

| Capability            | Evidence                                                                        |
| --------------------- | ------------------------------------------------------------------------------- |
| **Token estimation**  | `compaction.c:67–80` — `hu_estimate_tokens`; multimodal tokens for images/audio |
| **Compaction**        | `compaction.c` — `hu_should_compact`, `hu_compact_history_llm`                  |
| **Context pressure**  | `agent_turn.c:604–622` — 85%/95% thresholds, auto-compact                       |
| **LLM summarization** | `compaction.c` — `hu_compact_history_llm`                                       |
| **Context tokens**    | `context_tokens.h` — model-specific limits                                      |

### MISSING

- **Hierarchical summarization** — compaction is single-level
- **Attention routing** — no learned priority over context segments
- **Priority-based context selection** — memory loader uses limit, not explicit priority

### Highest-Impact Missing

**Hierarchical summarization** — multi-level summarization (e.g. session → chapter → summary) would improve long-conversation retention.

---

## 8. Perception & Multimodal

**Rating: 4/5**

### EXISTS

| Capability            | Evidence                                                                                      |
| --------------------- | --------------------------------------------------------------------------------------------- |
| **Vision**            | `include/human/context/vision.h`, `provider.h` — `supports_vision`, `hu_content_part_image_*` |
| **Image description** | `hu_vision_describe_image`                                                                    |
| **Audio**             | `voice.h` — STT, TTS; `audio_pipeline.h`                                                      |
| **Multimodal config** | `multimodal.h` — `hu_multimodal_config_t`, max images, encoding                               |
| **Content parts**     | `provider.h` — `HU_CONTENT_PART_IMAGE_URL`, `AUDIO_BASE64`, etc.                              |

### MISSING

- **Document parsing** — PDF tool exists (`pdf.c`) but no general OCR/document understanding
- **Video understanding** — token estimate only |

### Highest-Impact Missing

**Document understanding** — OCR + structured extraction for PDFs/docs would improve knowledge ingestion.

---

## 9. Communication

**Rating: 4/5**

### EXISTS

| Capability                 | Evidence                                                                  |
| -------------------------- | ------------------------------------------------------------------------- |
| **Persona system**         | `persona.h` — identity, traits, overlays, example banks, contact profiles |
| **Channel overlays**       | `hu_persona_overlay_t` — formality, avg_length, emoji_usage per channel   |
| **Emotional intelligence** | `superhuman_emotional.c` — crisis/distress keywords                       |
| **Theory of mind**         | `theory_of_mind.c` — `hu_belief_state_t`, beliefs                         |
| **Style adaptation**       | Persona overlays, tone detection                                          |
| **Circadian**              | `circadian.c` — time-of-day awareness                                     |
| **Relationship**           | `relationship.c` — relationship stage                                     |

### MISSING

- **Deep emotional modeling** — crisis detection is keyword-based
- **Proactive empathy** — limited to check-ins and starters

### Highest-Impact Missing

**Learned emotional modeling** — replacing keyword lists with learned emotion detection would improve empathy.

---

## 10. Infrastructure

**Rating: 4/5**

### EXISTS

| Capability        | Evidence                                                  |
| ----------------- | --------------------------------------------------------- |
| **Streaming**     | `provider.h` — `stream_chat`, `hu_stream_chunk_t`         |
| **Caching**       | `prompt.c` — cached static prompt; `lifecycle/cache.c`    |
| **Observability** | `observer.h`, `bth_metrics.h`, `otel.h` — events, metrics |
| **Benchmarking**  | `benchmark.yml` — binary size, startup, RSS               |

### MISSING

- **Response caching** — no semantic cache for repeated queries
- **Cost tracking** — `hu_agent_internal_record_cost` exists but limited visibility

### Highest-Impact Missing

**Semantic response cache** — `lifecycle/semantic_cache.c` exists for memory; extending to response caching would reduce repeated queries.

---

## Summary Table

| Area                       | Rating | Highest-Impact Missing        |
| -------------------------- | ------ | ----------------------------- |
| 1. Reasoning & Planning    | 3/5    | Tree-of-thought / beam search |
| 2. Memory Architecture     | 4/5    | Learned importance prediction |
| 3. Tool Use                | 4/5    | LLMCompiler integration       |
| 4. Multi-Agent             | 2/5    | LLM-driven task decomposition |
| 5. Learning                | 3/5    | Reward modeling               |
| 6. Safety & Alignment      | 3/5    | Constitutional AI             |
| 7. Context Management      | 4/5    | Hierarchical summarization    |
| 8. Perception & Multimodal | 4/5    | Document understanding        |
| 9. Communication           | 4/5    | Learned emotional modeling    |
| 10. Infrastructure         | 4/5    | Semantic response cache       |

---

## Recommended Priorities

1. **Wire LLMCompiler** — `agent_turn.c` should call `hu_llm_compiler_run` when `llm_compiler_enabled` and 3+ tool calls, then use `dag_executor`.
2. **Add Constitutional AI** — inject principles into system prompt via config.
3. **Implement ToT** — add branching exploration for complex reasoning.
4. **LLM-driven orchestrator** — add `hu_orchestrator_decompose_goal` that calls provider to split goals.

---

## Key Files Reference

| Concept      | Files                                                                                          |
| ------------ | ---------------------------------------------------------------------------------------------- |
| Planner      | `src/agent/planner.c`                                                                          |
| Agent turn   | `src/agent/agent_turn.c`                                                                       |
| Orchestrator | `src/agent/orchestrator.c`, `include/human/agent/orchestrator.h`                               |
| Memory       | `src/memory/`, `include/human/memory.h`                                                        |
| Tools        | `src/tools/factory.c`, 67+ tools                                                               |
| Intelligence | `src/intelligence/` (self_improve, online_learning, value_learning, meta_learning, reflection) |
| Security     | `src/security/` (policy_engine, input_guard, sandbox)                                          |
| Context      | `src/agent/compaction.c`, `include/human/context_tokens.h`                                     |
| Channels     | 34 channels (CLAUDE.md)                                                                        |
| Persona      | `include/human/persona.h`, `src/persona/`                                                      |
