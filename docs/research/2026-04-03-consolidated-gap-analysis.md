---
title: "Consolidated Gap Analysis — h-uman vs SOTA (April 2026)"
created: 2026-04-03
status: active
scope: all dimensions
sources: 50+ papers, Karpathy autoresearch, h-uman eval suites, arXiv assessment
gaps: 26
---

# Consolidated Gap Analysis — h-uman vs SOTA

All known gaps across every dimension, synthesized from:
- h-uman's own arXiv assessment (13 Turing dimensions, March 2026)
- SOTA AGI Convergence plan (12 critical gaps, March 2026)
- SOTA research map (agents, skills, companions, March 2026)
- New arXiv research on behavioral patterns, memory, and cognition (April 2026)
- Karpathy's autoresearch framework analysis (April 2026)
- **NEW:** Companion AI safety frameworks (April 2026)
- **NEW:** Conversation repair & trust calibration (April 2026)
- **NEW:** Proactive behavior & humor generation (April 2026)

---

## CRITICAL Gaps (Must Close for AGI-Class)

### 1. Self-Improvement Loop (AGI-S3)

**Current:** Prompt patches, tool preferences. No closed-loop autonomous experimentation.
**SOTA Target:** Autoresearch-style loop — bounded experiments, single metric, keep/discard.
**Research:** [karpathy/autoresearch](https://github.com/karpathy/autoresearch)
**Action:** Build autoresearch-for-conversation: fixed eval harness + modifiable behavior file + composite Human Fidelity Score + 5-minute bounded runs.

### 2. Evaluation System (AGI-E1-E4)

**Current:** Phase 1 complete (eval runner, LLM judge, benchmarks, CI). Rated B.
**SOTA Target:** Continuous eval with longitudinal tracking, adversarial scenarios, cross-dimension composite score.
**Research:** [tau-bench](https://arxiv.org/abs/2406.12045), TwinVoice, PersonaTwin
**Action:** Add composite scalar metric for autoresearch loop. Add longitudinal eval scenarios.

### 3. World Model & Causal Reasoning (AGI-W1-W8)

**Current:** Causal graph engine built (Phase 2 in progress). BFS traversal, path finding.
**SOTA Target:** Simulative reasoning, counterfactual analysis, MCTS planning.
**Research:** MAGMA multi-graph, action-conditioned prediction
**Action:** Complete Phase 2 (simulation engine, recursive ToT, strategic reasoning).

### 4. Multi-Agent Swarm (AGI-O1-O7)

**Current:** Static orchestrator + mailbox.
**SOTA Target:** Dynamic parallel swarms, RL-trained coordination, budgeted spawning.
**Research:** Anthropic multi-agent (token budgets), OpenAI Agents SDK (handoffs vs agents-as-tools)
**Action:** Implement spawn budgets (P0 from research map), plan persistence, handoff tools.

---

## HIGH Gaps (Required for Human Fidelity)

### 5. Personality Consistency (Score: 6/10)

**Current:** Persona JSON + overlays. No drift detection or multi-turn RL.
**SOTA Target:** 3 metrics (prompt-to-line, line-to-line, Q&A) + multi-turn RL reducing drift by 55%.
**Research:** Multi-Turn RL for Persona Consistency (OpenReview 2025), BALLERINA identity containment
**Action:** Add 3 consistency metrics to eval rubric. Implement identity containment. Add drift detection.

### 6. Vulnerability Calibration (Score: 5/10 — LOWEST)

**Current:** No per-channel or per-contact vulnerability calibration.
**SOTA Target:** `vulnerability_tier` in persona overlay, per-channel calibration.
**Research:** [Disclosure By Design](https://arxiv.org/abs/2603.16874)
**Action:** Add `vulnerability_tier` to persona overlay. Calibrate disclosure rates per channel and role.

### 7. Memory → Adaptive Retrieval (Score: 7/10 but architecture gap)

**Current:** Multi-engine memory with SQLite, recall, core. No adaptive weighting.
**SOTA Target:** Multi-layered with adaptive layer-weighting mechanism per query type.
**Research:** [Multi-Layered Memory Architectures](https://arxiv.org/abs/2603.29194), [Memoria](https://arxiv.org/abs/2512.12686), [Memory in the Age of AI Agents](https://arxiv.org/abs/2512.13564)
**Action:** Implement query classifier (episodic vs semantic). Add adaptive retrieval weighting. Separate episodic/semantic paths. Add forgetting/decay curves. Add false memory prevention.

### 8. Affect Tracking & Mimicry Calibration

**Current:** Emotion detection exists but no mimicry ceiling or calibration.
**SOTA Target:** Calibrated affect mirroring with per-channel intensity limits.
**Research:** [Illusions of Intimacy](https://arxiv.org/abs/2505.11649)
**Action:** Add `affect_mirror_ceiling` to persona overlay. Don't amplify blindly. Calibrate per contact vulnerability level.

### 9. Longitudinal Behavioral Tracking

**Current:** Message metrics in `contact_baselines`. No attachment trajectory.
**SOTA Target:** Multi-week tracking of attachment, dependency, empathy perception changes.
**Research:** [Longitudinal AI Agent Study](https://arxiv.org/abs/2504.14112) — 33% attachment increase in 5 weeks
**Action:** Add attachment trajectory to `contact_baselines`. Add healthy attachment monitor. Add dependency alert thresholds. Track the 5-week inflection point.

### 10. Long-Term Autonomy (AGI Gap #8)

**Current:** Cron + commitments.
**SOTA Target:** Infinite-horizon, bounded context, intrinsic motivation.
**Research:** MemGPT paging policies, SIMA 2 self-improvement in new environments
**Action:** Document paging policy. Add intrinsic motivation signals. Measure long-session eval regression.

---

## MEDIUM Gaps

### 11. Humor & Affective Expression (Score: 6/10)

**Current:** No DPO training on humor-positive pairs.
**SOTA Target:** DPO on humor pairs, mine persona examples, reduce affective tone gap.
**Research:** [Computational Turing Test](https://arxiv.org/abs/2511.04195) — affective expression is primary AI tell
**Action:** Collect humor-positive conversation pairs. Apply DPO. Mine examples from persona banks.

### 12. Opinion Having & Anti-Sycophancy (Score: 6/10)

**Current:** Opinion table exists but no structured reasoning before expression.
**SOTA Target:** Chain-of-thought reasoning + RL on opinion datasets.
**Research:** [Reasoning Boosts Opinion Alignment](https://arxiv.org/abs/2603.01214), [LLM Sycophancy Under Rebuttal](https://arxiv.org/abs/2509.16533)
**Action:** Add sycophancy detection to metacognition. Implement reasoning-before-opinion pattern. Note: casual feedback shifts opinions more than formal critiques (important for channel design).

### 13. Multimodal / Omni-Modal (AGI Gap #6)

**Current:** Image description shim.
**SOTA Target:** Native cross-modal reasoning, active perception.
**Action:** Implement Qwen3-Omni-style cross-modal reasoning.

### 14. Computer Use / GUI Agent (AGI Gap #9)

**Current:** Tool invocation only.
**SOTA Target:** Visual GUI agents, reflection-memory, OSWorld-class.
**Action:** Phase 6 of SOTA convergence plan.

### 15. Bounded Rationality in Conversation Design

**Current:** Conversation design assumes rational users.
**SOTA Target:** Design that works with human heuristics (anchoring, availability, framing).
**Research:** [Bounded Minds, Generative Machines](https://arxiv.org/abs/2601.13376)
**Action:** Account for anchoring effects in option presentation. Leverage framing intentionally in `opinion_having`.

---

## CRITICAL Gaps — NEW (Found in April 2026 Review)

### 19. Companion AI Safety Monitoring (COMPLETELY MISSING)

**Current:** Security standards exist. Zero companion-specific safety monitoring.
**SOTA Target:** SHIELD-style 5-dimension supervisor running on every response.
**Research:** [SHIELD](https://arxiv.org/abs/2510.15891) — 50-79% reduction in concerning content. [EmoAgent](https://arxiv.org/abs/2504.09689) — 34.4% psychological deterioration in vulnerable users. [Emotional Manipulation](https://arxiv.org/abs/2508.19258) — 37% of farewells use manipulation tactics.
**Action:** Build companion safety supervisor module. Add vulnerable user eval personas. Anti-manipulation rules for farewells. Crisis escalation to humans.

### 20. Conversation Repair & Error Recovery (COMPLETELY MISSING)

**Current:** No repair mechanism. No error acknowledgment. No divergence detection.
**SOTA Target:** Repair strategy library, self-correction, "I got confused" capability.
**Research:** [Invisible Failures](https://arxiv.org/abs/2603.15423) — many failures go undetected. [LLMs Get Lost](https://arxiv.org/abs/2505.06120) — LLMs compound errors rather than recovering. [Repair Taxonomy](https://www.researchgate.net/publication/395190410) — taxonomy of repair strategies.
**Action:** Build repair module. Detect user repair attempts. Self-verify memory claims before presenting. Explicit error acknowledgment.

### 21. Memory Hallucination Grounding (COMPLETELY MISSING)

**Current:** Memory recall has no verification step. Can fabricate shared experiences.
**SOTA Target:** Self-verify personal claims before presenting. "Did this event actually happen?"
**Research:** [Invisible Failures](https://arxiv.org/abs/2603.15423) — silent trust erosion from undetected errors.
**Action:** Add memory verification step before any "I remember when you..." claim. This is THE most trust-damaging failure mode for a "close friend."

## HIGH Gaps — NEW (Found in April 2026 Review)

### 22. Trust Calibration (COMPLETELY MISSING)

**Current:** No trust metric. No trust erosion detection. No confidence-to-language mapping.
**SOTA Target:** Trust maturity model (5 dimensions), Mutual ToM for trust, calibrated uncertainty language.
**Research:** [TCMM](https://arxiv.org/abs/2503.15511) — 5-dimension trust model. [MToM Trust](https://arxiv.org/abs/2601.16960) — model what user thinks AI knows. [Miscalibrated Confidence](https://arxiv.org/abs/2402.07632) — poorly calibrated confidence increases inappropriate reliance.
**Action:** Build trust module. Map internal confidence to language uncertainty markers. Model user's beliefs about what h-uman knows (bidirectional ToM).

### 23. Proactive / Anticipatory Social Intelligence (THIN)

**Current:** Cron + commitments. Fixed hourly proactive check.
**SOTA Target:** Continuous context monitoring, inner thoughts accumulation, "right moment" detection.
**Research:** [ProAgentBench](https://arxiv.org/abs/2602.04482) — memory enhances proactive prediction. [Proactive Inner Thoughts](https://arxiv.org/abs/2501.00383) — agents formulate anticipatory state.
**Action:** Build anticipatory state model per contact. Background inner thoughts between conversations. Timing/relevance scoring for proactive reach-outs.

### 24. Humor Generation Framework (COMPLETELY MISSING)

**Current:** Scored 6/10 with no structured humor capability.
**SOTA Target:** 5-component cognitive humor model (reasoning, social, knowledge, creative, audience).
**Research:** [AI Humor Generation](https://arxiv.org/abs/2502.07981) — humor skills approach performs near human-level. [Computational Humour Survey](https://arxiv.org/abs/2509.21175). [MWAHAHA SemEval 2026](https://www.aclweb.org/portal/content/semeval-2026-task-1-mwahaha-models-write-automatic-humor-and-humans-annotate).
**Action:** Build humor engine. Persona-specific humor style. Audience-aware humor. Failed humor recovery. Add humor to eval rubric.

## MEDIUM Gaps — NEW (Found in April 2026 Review)

### 25. Temporal Reasoning & Life Stage Awareness (MISSING)

**Current:** `life_chapters` table exists but no temporal reasoning about seasons, anniversaries, transitions.
**SOTA Target:** Temporal context awareness — know what time of year it is, what life stage the user is in, what's coming up.
**Action:** Wire temporal context into conversation awareness. Birthday/anniversary/seasonal awareness. Life transition detection.

### 26. Relationship Development Stages (MISSING)

**Current:** Phase 6 targets "close friend" but no progression model (acquaintance → friend → close friend).
**SOTA Target:** Relationship maturity scaling — vulnerability, humor, informality should scale with relationship depth.
**Action:** Define relationship stages. Scale disclosure/humor/informality to relationship maturity. Measure progression over time.

---

## LOW Gaps (Nice-to-Have)

### 16. Agent Behavioral Science Framework

**Current:** No formal theoretical framework for evaluation.
**Research:** [AI Agent Behavioral Science](https://arxiv.org/abs/2506.06366) — social cognitive theory: intrinsic/environmental/behavioral
**Action:** Adopt three-dimension framework for structuring eval dimensions.

### 17. Memory Consolidation During Idle

**Current:** Proactive check cycle runs hourly but doesn't consolidate memory.
**Research:** [AI Meets Brain](https://arxiv.org/abs/2512.23343) — brain consolidates during sleep/idle
**Action:** Add explicit memory consolidation step to proactive check cycle.

### 18. Code Execution Sandbox (AGI Gap #10)

**Current:** Multi-backend sandbox exists.
**SOTA Target:** Ephemeral microVMs, checkpoint/restore.
**Action:** Phase 6 of SOTA convergence plan.

---

## Dimensions Scorecard (Updated April 2026)

| # | Dimension | Score | Trend | Primary Gap |
| --- | --- | --- | --- | --- |
| 1 | `natural_language` | 8/10 | → | Reduce "helpful assistant" framing on casual channels |
| 2 | `emotional_intelligence` | 7/10 | ↑ | Add trajectory empathy scorer (EMPA) |
| 3 | `appropriate_length` | 8/10 | → | Already SOTA — monitor |
| 4 | `personality_consistency` | 6/10 | ↑ | 3 metrics + multi-turn RL + identity containment |
| 5 | `vulnerability_willingness` | 5/10 | ! | Per-channel vulnerability tier (LOWEST — priority) |
| 6 | `humor_naturalness` | 6/10 | → | DPO on humor-positive pairs |
| 7 | `imperfection` | 7/10 | → | Already SOTA — extend patterns |
| 8 | `opinion_having` | 6/10 | ↑ | Anti-sycophancy + reasoning-before-opinion |
| 9 | `energy_matching` | 7/10 | → | Track message intensity/rhythm in style tracker |
| 10 | `context_awareness` | 7/10 | → | Already addressed by memory + ToM pipeline |
| 11 | `non_robotic` | 8/10 | → | Use less-aligned model tiers for casual channels |
| 12 | `genuine_warmth` | 7/10 | ↑ | Affect mirror ceiling + contact profiles |
| 13 | `orchestration_quality` | 7/10 | → | Track via HuLa trace success rate |

**Composite Score: 6.85/10** (unweighted average)
**Weighted Priority Score:** vulnerability_willingness (5), personality_consistency (6), humor_naturalness (6), opinion_having (6) need the most work.

---

## Research File Index

| File | Coverage |
| --- | --- |
| `2026-03-23-human-fidelity-arxiv-assessment.md` | 13 Turing dimensions × 10 arXiv papers |
| `2026-03-20-sota-agents-skills-companion.md` | Lab research synthesis (Anthropic, OpenAI, DeepMind, etc.) |
| `2026-04-03-autoresearch-framework.md` | Karpathy's autoresearch analysis + implications |
| `2026-04-03-conversational-behavioral-patterns.md` | 7 papers on emotional dynamics, attachment, affect |
| `2026-04-03-memory-cognition-systems.md` | 8 papers on memory architectures, cognitive science |
| `2026-04-03-companion-safety-frameworks.md` | 6 papers on companion safety, manipulation, dependency |
| `2026-04-03-conversation-repair-trust.md` | 7 papers on trust calibration, error recovery, repair |
| `2026-04-03-proactive-behavior-humor.md` | 6 papers on anticipatory agents, humor generation |
| `2026-04-03-consolidated-gap-analysis.md` | This file — master gap list (26 gaps) |

## Cross-Reference: Plans

| Plan | Gaps Addressed |
| --- | --- |
| `2026-03-15-sota-agi-convergence.md` | #1-4, #10, #13-14, #18 (12 AGI gaps, 6 phases) |
| `2026-03-10-human-fidelity-phase6-agi-cognition.md` | #5, #6, #8, #9, #11, #12 (cognition layer) |
| `2026-03-21-dual-process-cognition.md` | #3 (System 1/System 2 mode switching) |
| `2026-03-21-emotional-cognition.md` | #8, #11 (emotional intelligence architecture) |
