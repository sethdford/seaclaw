---
title: "Conversation Repair & Trust Calibration — arXiv Research Synthesis"
created: 2026-04-03
status: active
scope: trust formation, repair after mistakes, dialogue recovery, hallucination grounding
papers: 7
severity: HIGH — trust and repair were completely absent from research
---

# Conversation Repair & Trust Calibration — arXiv Research Synthesis

Two intertwined gaps: (1) How to build and maintain trust in AI companions, and (2) How to repair conversations when the AI makes mistakes. Both are essential for the "close friend test."

---

## Part 1: Trust Calibration

### 1. Trust Calibration Maturity Model (March 2025)

**Paper:** [arXiv:2503.15511](https://arxiv.org/abs/2503.15511)
**Title:** "The Trust Calibration Maturity Model for Characterizing and Communicating Trustworthiness of AI Systems"

**Five Trust Dimensions:**
1. Performance Characterization — does the system do what it claims?
2. Bias & Robustness Quantification — does it handle edge cases?
3. Transparency — can the user understand why it did something?
4. Safety & Security — does it protect the user?
5. Usability — is the interaction natural?

**Key Insight:** Trust information becomes less accessible as systems grow in scale, creating risk of inappropriate use.

**Implications for h-uman:**
- We measure conversation quality but NOT trust calibration
- Need: "does the user trust h-uman appropriately?" metric
- Over-trust is as dangerous as under-trust (see: attachment inflation in longitudinal study)

---

### 2. Miscalibrated AI Confidence and User Trust (2024, updated 2025)

**Paper:** [arXiv:2402.07632](https://arxiv.org/abs/2402.07632)
**Title:** "Understanding the Effects of Miscalibrated AI Confidence on User Trust, Reliance, and Decision Efficacy"

**Key Finding:** Presenting AI confidence scores helps calibrate trust — **but only when confidence is well-calibrated**. Poorly calibrated confidence increases inappropriate reliance.

**Implications for h-uman:**
- When h-uman expresses uncertainty ("I think..." vs "I remember..."), calibration matters
- Our `imperfection` dimension (7/10) already uses hedging — but is it calibrated to actual confidence?
- Need: map internal confidence to language uncertainty markers

---

### 3. Mutual Theory of Mind and Trust (January 2026)

**Paper:** [arXiv:2601.16960](https://arxiv.org/abs/2601.16960)
**Title:** "Do We Know What They Know We Know? Calibrating Trust Through Mutual Theory of Mind"

**Key Insight:** Trust calibration improves when both parties model each other's knowledge states (MToM).

**Implications for h-uman:**
- Phase 6 ToM models the user's mental state
- For trust: also model what the user *thinks h-uman knows*
- If user thinks h-uman remembers something it doesn't, that's a trust violation waiting to happen

---

## Part 2: Conversation Repair

### 4. The Art of Repair in Human-Agent Conversations (ResearchGate, 2025)

**Source:** [ResearchGate](https://www.researchgate.net/publication/395190410)
**Title:** "The Art of Repair in Human-Agent Conversations: A Taxonomy of Repair Strategies by Users and LLM-Based Conversational Agents"

**Taxonomy:** Analyzes 21 chat logs with contextual interviews. Categorizes how users identify, interpret, and attempt to repair problematic AI outputs.

**Implications for h-uman:**
- Need repair strategy library: acknowledge error, correct claim, rebuild context
- Users attempt repair before giving up — detect repair attempts and cooperate
- The taxonomy should inform our conversation flow design

---

### 5. Invisible Failures in Human-AI Interactions (March 2026)

**Paper:** [arXiv:2603.15423](https://arxiv.org/abs/2603.15423)
**Title:** "Invisible Failures in Human-AI Interactions"

**Key Finding:** Many AI failures go undetected by users — they accept wrong answers or move on without signaling a problem. These "invisible failures" erode trust silently.

**Implications for h-uman:**
- Can't rely on user feedback to catch errors — need internal consistency checking
- Memory recall should self-verify before presenting: "Did this event actually happen?"
- **Hallucination grounding for personal facts** is the single most trust-damaging failure mode
- A close friend who fabricates shared memories is worse than one who forgets

---

### 6. Dialogue Repair in Voice Assistants (November 2023, updated 2025)

**Paper:** [arXiv:2311.03952](https://arxiv.org/abs/2311.03952)
**Title:** "An Analysis of Dialogue Repair in Voice Assistants"

Examines "huh?" as a repair strategy — how Google Assistant and Siri handle other-initiated repair.

**Implications for h-uman:**
- Voice channel needs explicit repair handling
- "What?" / "Huh?" / "That's not what I said" should trigger repair, not repetition

---

### 7. LLMs Get Lost in Multi-Turn Conversations (May 2025)

**Paper:** [arXiv:2505.06120](https://arxiv.org/abs/2505.06120)

**Key Finding:** When LLMs take a wrong turn in a conversation, they get lost and **do not recover** — they compound the error rather than self-correcting.

**Implications for h-uman:**
- Need explicit error detection and recovery mechanism
- If h-uman detects it's gone off-track, it should acknowledge and reset rather than compound
- The "close friend" behavior: "Wait, I think I got confused. Let me back up."

---

## Gap Analysis: Trust & Repair

| Capability | SOTA | h-uman Current | Gap |
| --- | --- | --- | --- |
| Trust calibration metric | TCMM 5 dimensions | None | **Missing entirely** |
| Confidence-language mapping | Calibrated uncertainty markers | `imperfection` dimension has hedging | Partially aligned — needs calibration |
| MToM for trust | Model user's beliefs about AI's knowledge | ToM models user's mental state only | **Gap** — one-directional |
| Repair strategy library | Taxonomy of repair approaches | None | **Missing entirely** |
| Invisible failure detection | Internal consistency checking | None | **Missing entirely** |
| Memory hallucination prevention | Self-verify before presenting | No verification | **Critical gap** |
| Error compounding prevention | Explicit reset mechanism | None | **Missing entirely** |
| Farewell manipulation avoidance | Anti-manipulation patterns | No explicit rules | **Missing** (see companion safety doc) |

---

## Proposed h-uman Additions

### Trust Module (`src/intelligence/trust.c`)

```
hu_trust_calibrate(contact_id)  — assess current trust level
hu_trust_detect_erosion()       — silent failure detection
hu_trust_repair_strategy()      — select repair approach
hu_trust_confidence_map()       — internal confidence → language markers
```

### Conversation Repair Module (`src/context/repair.c`)

```
hu_repair_detect_divergence()   — detect when conversation went off-track
hu_repair_acknowledge_error()   — "Wait, I think I got confused"
hu_repair_memory_verify()       — check memory claim before presenting
hu_repair_cooperate()           — detect user repair attempts
```
