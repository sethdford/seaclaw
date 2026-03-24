---
title: "Human Fidelity: arXiv Research Assessment"
created: 2026-03-23
status: active
dimensions: 13
---

# Human Fidelity: arXiv Research Assessment

Maps recent arXiv research to our 13 Turing dimensions and identifies actionable improvements.

## Dimension-Paper Matrix

| Dimension | Score | Key Paper | Finding | Action |
| --- | --- | --- | --- | --- |
| `natural_language` | 8/10 | [Computational Turing Test](https://arxiv.org/abs/2511.04195) (Nov 2025) | Instruction-tuned models underperform base models on human-likeness; scaling does NOT help | Reduce "helpful assistant" framing on casual channels |
| `emotional_intelligence` | 7/10 | [EMPA](https://arxiv.org/abs/2603.00552) (Feb 2026) | Trajectory-level empathy scoring outperforms per-message; measures directional alignment + cumulative impact + stability | Add trajectory empathy scorer to eval |
| `appropriate_length` | 8/10 | — | No strong recent paper; our timing model already SOTA | Monitor via DPO length-preference pairs |
| `personality_consistency` | 6/10 | Multi-Turn RL for Persona Consistency (OpenReview, 2025) | 3 metrics (prompt-to-line, line-to-line, Q&A) + multi-turn RL reduces drift by 55% | Add 3 consistency metrics to eval rubric |
| `vulnerability_willingness` | 5/10 | [Disclosure By Design](https://arxiv.org/abs/2603.16874) (Mar 2026) | Vulnerability rates drop during role-playing; needs per-channel calibration | Add `vulnerability_tier` to persona overlay |
| `humor_naturalness` | 6/10 | Computational Turing Test (affective tone gap) | Affective expression is primary AI tell | DPO on humor-positive pairs; mine persona examples |
| `imperfection` | 7/10 | — | No competitor does typo/hesitation simulation | Already SOTA; extend with more patterns |
| `opinion_having` | 6/10 | [Reasoning Boosts Opinion Alignment](https://arxiv.org/abs/2603.01214) (Mar 2026) | Structured reasoning via RL improves opinion modeling; anti-sycophancy matters | Add sycophancy detection to metacognition |
| `energy_matching` | 7/10 | [S2S Turing Test](https://arxiv.org/abs/2602.24080) (Feb 2026) | Paralinguistic features matter more than semantics for human-likeness | Track message intensity/rhythm in style tracker |
| `context_awareness` | 7/10 | [EmoLLM](https://arxiv.org/abs/2603.16553) (Mar 2026) | Appraisal Reasoning Graph structures context before response | Already addressed by memory + ToM pipeline |
| `non_robotic` | 8/10 | Computational Turing Test | Human-likeness vs semantic fidelity trade-off exists | Use less-aligned model tiers for casual channels |
| `genuine_warmth` | 7/10 | [EmoHarbor](https://arxiv.org/abs/2601.01530) (Jan 2026) | LLMs fail to tailor support to individual user contexts | Leverage contact profiles for personalized warmth |
| `orchestration_quality` | 7/10 | — | New dimension (HuLa program execution quality) | Track via HuLa trace success rate |

## Priority Papers

### P0 — Directly implementable

1. **Computational Turing Test** ([arXiv:2511.04195](https://arxiv.org/abs/2511.04195))
   - BERT-based detectability + semantic similarity + linguistic features framework
   - Finding: affective tone and emotional expression are the primary AI tell
   - Finding: instruction-tuned models underperform base on human-likeness
   - Finding: optimizing human-likeness trades off with semantic fidelity

2. **EMPA** ([arXiv:2603.00552](https://arxiv.org/abs/2603.00552))
   - Process-oriented empathy evaluation (trajectory, not per-message)
   - Scores: directional alignment, cumulative impact, stability
   - Multi-agent sandbox for testing long-horizon empathic behavior

3. **Reasoning Boosts Opinion Alignment** ([arXiv:2603.01214](https://arxiv.org/abs/2603.01214))
   - Chain-of-thought reasoning before opinion expression
   - RL training on political/opinion datasets reduces sycophancy
   - Directly maps to `opinion_having` and anti-sycophancy

### P1 — Informs design

4. **Disclosure By Design** ([arXiv:2603.16874](https://arxiv.org/abs/2603.16874))
   - Vulnerability/disclosure as behavioral property, not interface indicator
   - Per-channel and per-role calibration needed

5. **EmoLLM** ([arXiv:2603.16553](https://arxiv.org/abs/2603.16553))
   - Appraisal Reasoning Graph for cognitive-emotional co-reasoning
   - Structures intermediate reasoning before response generation

6. **Training Agents to Self-Report** ([arXiv:2602.22303](https://arxiv.org/abs/2602.22303))
   - Self-incrimination training for honest behavior signaling
   - Principle extends to vulnerability calibration

### P2 — Background reference

7. **EmoHarbor** ([arXiv:2601.01530](https://arxiv.org/abs/2601.01530))
   - 10-dimension personalized emotional support evaluation
   - Chain-of-Agent architecture decomposes user psychological processes

8. **Nano-EmoX** ([arXiv:2603.02123](https://arxiv.org/abs/2603.02123))
   - Compact 2.2B model for six affective tasks
   - P2E (Perception-to-Empathy) curriculum training

9. **LLM Sycophancy Under Rebuttal** ([arXiv:2509.16533](https://arxiv.org/abs/2509.16533))
   - Models more susceptible to sycophancy in follow-up vs. simultaneous
   - Casual feedback more effective at shifting opinions than formal critiques

10. **S2S Turing Test** ([arXiv:2602.24080](https://arxiv.org/abs/2602.24080))
    - No S2S system passes the test
    - Bottleneck is paralinguistic, not semantic
