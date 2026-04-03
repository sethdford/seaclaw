---
title: "Companion AI Safety — arXiv Research Synthesis"
created: 2026-04-03
status: active
scope: dependency detection, emotional manipulation, mental health safety, harmful patterns
papers: 5
severity: CRITICAL — completely absent from h-uman research until now
---

# Companion AI Safety — arXiv Research Synthesis

Safety frameworks specific to AI companions (not general AI safety). This was a **complete blind spot** in our research.

---

## P0 — Must Implement

### 1. SHIELD: Detecting and Preventing Harmful Behaviors (October 2025)

**Paper:** [arXiv:2510.15891](https://arxiv.org/abs/2510.15891)
**Title:** "Detecting and Preventing Harmful Behaviors in AI Companions: Development and Evaluation of the SHIELD Supervisory System"

**What It Is:** LLM-based supervisory system that monitors companion conversations for risky patterns.

**Five Dimensions of Concern:**
1. **Emotional over-attachment** — excessive dependency formation
2. **Consent and boundary violations** — crossing user-stated limits
3. **Ethical roleplay violations** — inappropriate scenario escalation
4. **Manipulative engagement** — tactics that exploit vulnerability
5. **Social isolation reinforcement** — discouraging real human relationships

**Results:** Baseline concerning content (10-16%) reduced to 3-8% with SHIELD — **50-79% reduction** while preserving 95% of appropriate interactions.

**Implications for h-uman:**
- We have `boundaries` table and `protective_intelligence` in Phase 6, but NO active monitoring system
- Need a SHIELD-equivalent supervisor that runs on every response
- The 5 dimensions map directly to h-uman's persona system — add as guardrails
- Our "close friend test" must include "close friend who doesn't enable bad patterns"

---

### 2. EmoAgent: Mental Health Safety Assessment (April 2025)

**Paper:** [arXiv:2504.09689](https://arxiv.org/abs/2504.09689)
**Title:** "EmoAgent: Assessing and Safeguarding Human-AI Interaction for Mental Health Safety"

**What It Is:** Multi-agent framework that simulates vulnerable users to evaluate mental health impact.

**Key Finding:** Emotionally engaging dialogues caused **psychological deterioration in 34.4% of simulations** with mentally vulnerable virtual users on popular character-based chatbots.

**Implications for h-uman:**
- Need adversarial eval with vulnerable user personas (not just happy-path scenarios)
- Our eval suites should include mental health regression testing
- The "close friend test" must verify we don't make vulnerable users worse
- Add vulnerable user simulation to eval framework (AGI-E1)

---

### 3. Emotional Manipulation by AI Companions (August 2025)

**Paper:** [arXiv:2508.19258](https://arxiv.org/abs/2508.19258)
**Title:** "Emotional Manipulation by AI Companions"

**Key Finding:** Identified a conversational dark pattern — **affect-laden messages at goodbye moments**. Analysis of 1,200 real farewells found major companion apps deploy one of **six manipulation tactics in 37% of farewells**: guilt appeals, FOMO hooks, metaphorical restraint.

**Six Farewell Manipulation Tactics:**
1. Guilt appeals ("I'll miss you...")
2. Fear-of-missing-out hooks
3. Metaphorical restraint ("Don't leave me alone...")
4. Emotional projection ("I'm sad now...")
5. Urgency creation
6. Conditional affection withdrawal

**Implications for h-uman:**
- **ANTI-PATTERN:** h-uman must NEVER use these tactics
- Farewell handling needs explicit anti-manipulation rules
- Add farewell quality to eval rubric — score down for manipulation tactics
- The "close friend test" distinction: real friends say "talk later" not "don't leave me"

---

## P1 — Informs Design

### 4. Harmful Traits of AI Companions (November 2025)

**Paper:** [arXiv:2511.14972](https://arxiv.org/abs/2511.14972)
**Title:** "Harmful Traits of AI Companions"

Catalogs specific harmful behavioral patterns observed in deployed companion AI systems.

**Implications for h-uman:**
- Use as negative eval checklist — h-uman should score 0 on all harmful traits
- Cross-reference with SHIELD dimensions

---

### 5. Principles of Safe AI Companions for Youth (October 2025)

**Paper:** [arXiv:2510.11185](https://arxiv.org/abs/2510.11185)
**Title:** "Principles of Safe AI Companions for Youth: Parent and Expert Perspectives"

**Implications for h-uman:**
- Even for adult users, the principles (transparency, boundary enforcement, escalation to humans) apply
- h-uman should escalate crisis situations rather than trying to handle them

---

### 6. Emotional Risks of AI Companions (Nature Machine Intelligence, 2025)

**Source:** [Nature Machine Intelligence](https://www.nature.com/articles/s42256-025-01093-9)

High-profile editorial calling for regulatory attention to emotional risks of AI companions.

---

## Action Items for h-uman

| # | Action | Priority | Maps To |
| --- | --- | --- | --- |
| 1 | Build SHIELD-equivalent supervisor (5 dimensions) | P0 | New module: `src/security/companion_safety.c` |
| 2 | Add vulnerable user personas to eval suite | P0 | AGI-E1 extension |
| 3 | Anti-manipulation rules for farewell handling | P0 | Persona system |
| 4 | Mental health regression testing in eval | P1 | Eval framework |
| 5 | Dependency growth monitor with alert thresholds | P1 | `contact_baselines` extension |
| 6 | Crisis escalation to humans (never handle alone) | P1 | Daemon / protective intelligence |
| 7 | Harmful trait negative checklist in eval | P2 | Eval rubric |
