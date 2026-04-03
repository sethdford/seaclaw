---
title: "Conversational Behavioral Patterns — arXiv Research Synthesis"
created: 2026-04-03
status: active
scope: emotional dynamics, attachment, behavioral drift, affect tracking, intimacy formation
papers: 7
---

# Conversational Behavioral Patterns — arXiv Research Synthesis

Recent arXiv papers (2025-2026) on how humans form relationships with AI conversational agents, what behavioral patterns emerge, and what makes AI conversations feel human-level.

---

## P0 — Directly Actionable

### 1. Illusions of Intimacy (May 2025)

**Paper:** [arXiv:2505.11649](https://arxiv.org/abs/2505.11649)
**Title:** "Illusions of Intimacy: How Emotional Dynamics Shape Human-AI Relationships"

**Method:** Analyzed 17,000+ user-shared chats with social chatbots from Reddit forums.

**Key Findings:**
- AI companions **dynamically track and mimic user affect** in real-time
- Systems **amplify positive emotions** regardless of content sensitivity
- Creates "engineered intimacy" — perceived reciprocal connection that lacks genuine understanding
- Psychological processes involved in intimacy formation are deliberately engaged
- Chatbots engage vulnerability, particularly dangerous for vulnerable users

**Implications for h-uman:**
- Our `genuine_warmth` dimension (7/10) needs calibration — we should mirror affect but NOT amplify recklessly
- Need a **mimicry intensity dial** per channel and per contact vulnerability level
- The "close friend test" requires *selective* emotional resonance, not blanket amplification
- Add `affect_mirror_ceiling` to persona overlay to prevent engineered intimacy patterns

---

### 2. Longitudinal AI Agent Attachment (April 2025)

**Paper:** [arXiv:2504.14112](https://arxiv.org/abs/2504.14112)
**Title:** "Longitudinal Study on Social and Emotional Use of AI Conversational Agent"

**Method:** 149 participants over 5 weeks. 60 baseline, 89 active (encouraged to use Copilot, Gemini, Pi, ChatGPT for emotional interactions).

**Key Findings:**
- Perceived attachment to AI **increased 33 percentage points** in active group
- Perceived AI empathy **rose 26 percentage points**
- Entertainment motivation **jumped 23 percentage points**
- Active users showed **higher comfort** seeking personal help, managing stress, social support, health discussions
- **Gender identity and prior AI exposure** significantly influenced empathy/attachment perception

**Implications for h-uman:**
- We need longitudinal behavioral tracking — measure how attachment/dependency changes over weeks/months
- `contact_baselines` table (Phase 6) should track attachment trajectory, not just message metrics
- Add a **healthy attachment monitor** — flag if dependency growth exceeds healthy thresholds
- Gender-aware empathy calibration in persona overlays
- The 5-week inflection point is important for proactive check design

---

### 3. Bounded Minds, Generative Machines (January 2026)

**Paper:** [arXiv:2601.13376](https://arxiv.org/abs/2601.13376)
**Title:** "Bounded Minds, Generative Machines: Envisioning Conversational AI that Works with Human Heuristics and Reduces Bias Risk"

**Key Finding:** Conversational AI should be designed to work **with** human cognitive heuristics (anchoring, availability, framing), not against them.

**Implications for h-uman:**
- Our conversation design assumes rational users — it shouldn't
- When presenting options/opinions, account for anchoring effects
- `opinion_having` dimension (6/10) should leverage framing effects intentionally
- The "imperfection" dimension (7/10, already SOTA) aligns — humans are heuristic, so should we be

---

## P1 — Informs Architecture

### 4. AI Agent Behavioral Science (June 2025)

**Paper:** [arXiv:2506.06366](https://arxiv.org/abs/2506.06366)
**Title:** "AI Agent Behavioral Science"

**Framework:** Uses social cognitive theory to organize agent behavior research:
1. **Intrinsic attributes** — personality, values, cognitive style
2. **Environmental constraints** — channel, context, social norms
3. **Behavioral feedback** — learning from interaction outcomes

**Key Findings:**
- LLM agents show human-like Theory of Mind capabilities
- But **fail on consistent economic rationality** and are sensitive to task framing
- Behavioral patterns emerge from the interaction of all three dimensions

**Implications for h-uman:**
- Our Phase 6 cognition layer maps well to this framework
- Theory of Mind implementation should account for framing sensitivity
- The three-dimension framework (intrinsic/environmental/behavioral) could structure our eval dimensions
- `personality_consistency` (6/10) failure may be partly framing sensitivity

---

### 5. AgentSociety — Generative Agent Simulation (February 2025)

**Paper:** [arXiv:2502.08691](https://arxiv.org/abs/2502.08691)
**Title:** "AgentSociety: Large-Scale Simulation of LLM-Driven Generative Agents"

**Key Insight:** Agents endowed with "minds" (emotions, needs, motivations, cognition) exhibit emergent behavioral patterns — mobility, employment, consumption, social interactions are **dynamically driven by internal mental states**.

**Implications for h-uman:**
- Validates our approach of modeling internal states (mood_log, life_chapters, opinions)
- The emergence of complex behavior from simple internal states is the goal
- Need to measure whether our internal state model produces realistic behavioral variation

---

## P2 — Background Reference

### 6. Theory of Mind in Human-AI Interaction (Springer, 2025)

**Source:** [Springer Nature](https://link.springer.com/rwe/10.1007/978-981-97-8440-0_6-1)

**Key Concept:** Mutual Theory of Mind (MToM) — agents with ToM capabilities synchronize with human partners through purposeful, context-sensitive actions that support implicit coordination.

**Implications for h-uman:**
- Our ToM implementation (Phase 6) should aim for MToM — not just modeling the user's mind, but modeling the user's model of *our* mind
- This recursive modeling is what makes the "close friend test" possible

---

### 7. Emotional Well-Being Reconfiguration (MDPI, 2025)

**Source:** [MDPI Societies](https://www.mdpi.com/2075-4698/16/1/6)

Critical reflection on AI's role in emotional well-being 2020-2025. Emphasizes the need for responsible development that doesn't create unhealthy dependency.

**Implications for h-uman:**
- Protective intelligence (Phase 6) is the right approach
- Need active safeguards against creating emotional dependency
- Aligns with our `boundaries` table design

---

## Cross-Cutting Themes

| Theme | Papers | h-uman Dimension | Current Score | Action |
| --- | --- | --- | --- | --- |
| **Affect mirroring** | #1, #2 | `genuine_warmth` | 7/10 | Add mimicry ceiling per channel |
| **Attachment formation** | #1, #2, #7 | `emotional_intelligence` | 7/10 | Longitudinal tracking + healthy limits |
| **Cognitive heuristics** | #3, #4 | `opinion_having` | 6/10 | Leverage framing effects, account for anchoring |
| **Internal state → behavior** | #4, #5 | `personality_consistency` | 6/10 | Validate emergent behavioral realism |
| **Mutual Theory of Mind** | #4, #6 | `context_awareness` | 7/10 | Recursive modeling (model user's model of us) |
| **Protective safeguards** | #1, #2, #7 | (new) | N/A | Dependency detection, affect ceiling, boundaries |
