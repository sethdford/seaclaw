---
title: "Proactive Behavior & Humor Generation — arXiv Research Synthesis"
created: 2026-04-03
status: active
scope: anticipatory agents, proactive assistance, computational humor, timing
papers: 6
severity: HIGH — proactive behavior partially covered, humor completely absent
---

# Proactive Behavior & Humor Generation — arXiv Research Synthesis

Two distinct gaps that both relate to the "close friend test": (1) anticipating needs without being asked, and (2) making people laugh naturally.

---

## Part 1: Proactive / Anticipatory Behavior

### 1. ProAgentBench: Evaluating Proactive Assistance (February 2026)

**Paper:** [arXiv:2602.04482](https://arxiv.org/abs/2602.04482)
**Title:** "ProAgentBench: Evaluating LLM Agents for Proactive Assistance with Real-World Data"

**What It Is:** Benchmark where agents monitor user screen activities and contextual signals, proactively determining **when to intervene** and **how to assist**.

**Key Findings:**
- Both context and **long-term memory significantly enhance prediction accuracy**
- **Real-world training data substantially outperforms synthetic data**
- Proactive assistance requires understanding user routines, not just current state

**Implications for h-uman:**
- Our proactive check cycle (hourly with jitter) is primitive compared to continuous monitoring
- Memory-driven proactive behavior ("I know you have a big meeting tomorrow") requires tight memory-to-proactive coupling
- Eval should include "did h-uman reach out at the right time?" metrics

---

### 2. Proactive Conversational Agents with Inner Thoughts (December 2025)

**Paper:** [arXiv:2501.00383](https://arxiv.org/abs/2501.00383)
**Title:** "Proactive Conversational Agents with Inner Thoughts"

**Key Concept:** A proactive AI formulates **inner thoughts** during conversation and seeks **the right moment to contribute** — provides relevant input without requiring explicit cues.

**Particularly challenging in multi-party conversations** — timing and relevance both matter.

**Implications for h-uman:**
- h-uman's daemon loop sends messages but doesn't have "inner thoughts" that build up anticipatory state
- Need: running background model of "what should I bring up when I next hear from this person?"
- This is distinct from commitments/reminders — it's anticipatory social intelligence

---

### 3. Proactive Agent: Shifting from Reactive to Active (October 2024, updated 2025)

**Paper:** [arXiv:2410.12361](https://arxiv.org/abs/2410.12361)
**Title:** "Proactive Agent: Shifting LLM Agents from Reactive Responses to Active Assistance"

**Framework:** PARE (Proactive Agent Research Environment) — user simulation framework for developing agents that anticipate needs.

**Implications for h-uman:**
- Need a simulation environment to train/eval proactive behavior
- Can't rely on real user data alone — simulated scenarios cover edge cases

---

## Part 2: Humor Generation

### 4. AI Humor Generation: Cognitive, Social and Creative Skills (February 2025)

**Paper:** [arXiv:2502.07981](https://arxiv.org/abs/2502.07981)
**Title:** "AI Humor Generation: Cognitive, Social and Creative Skills for Effective Humor"

**Key Framework:** Humor requires:
1. **Cognitive reasoning** — understanding incongruity, surprise
2. **Social understanding** — audience awareness, appropriateness
3. **Knowledge breadth** — cultural references, shared context
4. **Creative thinking** — novelty, unexpected connections
5. **Audience understanding** — what THIS person finds funny

**Key Finding:** LLM captions enhanced with explicit humor skills are preferred by users and **perform almost on par with top-rated human humor**.

**Implications for h-uman:**
- Our `humor_naturalness` dimension (6/10) has no structured humor capability
- Need: humor skill integration with all 5 components
- Persona-specific humor style (what does h-uman find funny? what does Seth find funny?)
- Audience modeling is critical — generic humor fails the "close friend test"

---

### 5. Computational Humour: Overview (September 2025)

**Paper:** [arXiv:2509.21175](https://arxiv.org/abs/2509.21175)
**Title:** "Who's Laughing Now? An Overview of Computational Humour Generation and Explanation"

Comprehensive survey of computational humor approaches including generation, explanation, and evaluation.

---

### 6. MWAHAHA: SemEval 2026 Humor Task

**Source:** [ACL Web](https://www.aclweb.org/portal/content/semeval-2026-task-1-mwahaha-models-write-automatic-humor-and-humans-annotate)

Active shared task evaluating whether models can generate humor with human annotation. Results from January 2026.

**Implications for h-uman:**
- Benchmark results from MWAHAHA could inform our humor evaluation rubric
- Shared task data could train humor-specific components

---

## Gap Analysis

### Proactive Behavior

| Capability | SOTA | h-uman Current | Gap |
| --- | --- | --- | --- |
| Continuous context monitoring | ProAgentBench-style | Hourly proactive check | **Primitive** — needs continuous awareness |
| Inner thoughts / anticipatory state | Running background model | None | **Missing** |
| Memory-driven proactive reach-out | Memory + context prediction | Cron + commitments | **Significant gap** |
| Timing of intervention | "Right moment" detection | Fixed interval | **Missing** |
| Proactive eval metrics | "Did agent reach out at right time?" | None | **Missing** |

### Humor

| Capability | SOTA | h-uman Current | Gap |
| --- | --- | --- | --- |
| Humor generation framework | 5-component cognitive model | None | **Missing entirely** |
| Persona-specific humor style | Personality-infused humor | Generic | **Missing** |
| Audience-aware humor | Per-person humor modeling | None | **Missing** |
| Humor timing | "Right moment" detection | None | **Missing** |
| Failed humor recovery | Grace after flat joke | None | **Missing** |
| Humor in eval rubric | MWAHAHA benchmark | Not evaluated | **Missing** |

---

## Proposed h-uman Additions

### Proactive Intelligence (`src/daemon.c` extension)

```
hu_anticipatory_state_update()  — build anticipatory model per contact
hu_proactive_timing_check()     — is NOW the right moment?
hu_proactive_relevance_score()  — is this worth bringing up?
hu_inner_thoughts_accumulate()  — background state between conversations
```

### Humor Engine (`src/persona/humor.c` — new)

```
hu_humor_assess_audience()      — what does this person find funny?
hu_humor_generate_candidate()   — produce humor attempt
hu_humor_check_appropriateness() — is this appropriate for context/channel?
hu_humor_recover_from_failure() — graceful recovery if joke falls flat
```
