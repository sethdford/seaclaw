---
title: "Karpathy's Autoresearch — Framework Analysis & Implications for h-uman"
created: 2026-04-03
status: active
source: https://github.com/karpathy/autoresearch
stars: 53500
---

# Karpathy's Autoresearch — Framework Analysis

## What It Is

Autoresearch is an autonomous AI research framework by Andrej Karpathy that gives an AI agent a small but real LLM training setup and lets it experiment autonomously overnight. The agent modifies code, trains for 5 minutes, checks if the result improved, and repeats.

**Repository:** [github.com/karpathy/autoresearch](https://github.com/karpathy/autoresearch) (53.5k stars, March 2026)

## Core Architecture

| File | Role | Modified by Agent? |
| --- | --- | --- |
| `prepare.py` | Data prep, BPE tokenizer, dataloaders | No (fixed) |
| `train.py` | Full GPT model, Muon+AdamW optimizer, training loop (~630 lines) | **Yes — the only file agents touch** |
| `program.md` | Human-written research direction for the agent | No (but iterated by humans between runs) |

## The Autonomous Research Loop

```
1. Agent reads program.md (research direction)
2. Agent modifies train.py (architecture, hyperparams, optimization)
3. Train for exactly 5 minutes (wall clock, excluding startup/compilation)
4. Evaluate: val_bpb (validation bits-per-byte) — lower is better
5. Keep improvement OR discard change
6. Repeat autonomously
```

- ~12 experiments per hour
- ~100 distinct attempts overnight
- Metric is vocab-size-independent (fair comparison across architectures)

## Design Philosophy

**"One GPU, one file, one metric."**

- Fixed time budget ensures fair architectural comparison despite varying model sizes
- Results are platform-specific but optimized for individual hardware
- Self-contained: no external dependencies beyond PyTorch + a few packages
- No distributed training, no complex configs

## Key Insight: program.md as Optimizable Parameter

The human-written `program.md` is itself treated as an optimizable parameter. Humans iterate on *what they tell the agent to research* — treating the research direction as the outer optimization loop while the agent handles the inner loop.

This is a **meta-optimization pattern**: optimize the instructions, not just the code.

## Implications for h-uman

### Direct Applicability

| Autoresearch Concept | h-uman Equivalent | Gap |
| --- | --- | --- |
| `train.py` (single file agent modifies) | Eval suites, persona prompts, conversation rules | No equivalent single-file experimentation target |
| `val_bpb` (single metric) | Human Fidelity Score (13 dimensions) | Need a composite single metric for autonomous optimization |
| `program.md` (research direction) | `docs/plans/*.md` | Not structured for autonomous agent consumption |
| 5-minute time budget | No time-bounded eval runs | Need bounded eval execution for rapid iteration |
| Keep/discard loop | No automated A/B on conversation quality | Missing closed-loop self-improvement |

### What to Build (AGI-S3: Self-Improvement Loop)

1. **Autoresearch-for-conversation:** Define a fixed eval harness (like `prepare.py`) and a modifiable prompt/behavior file (like `train.py`). Let an agent iterate on conversation quality overnight.

2. **Composite Human Fidelity Metric:** Collapse the 13 Turing dimensions into a single scalar for autonomous optimization. Weight by severity (vulnerability_willingness and personality_consistency are lowest-scoring).

3. **Bounded Eval Runs:** Each experiment = run eval suite in <5 minutes, measure composite score, keep/discard.

4. **Research Direction File:** A `program.md` equivalent that steers what the self-improvement agent focuses on (e.g., "improve humor naturalness on casual channels").

### Architecture Sketch

```
research_direction.md  (human-written, iterated weekly)
        |
        v
conversation_rules.md  (the "train.py" — agent modifies this)
        |
        v
eval_runner (fixed)  →  run 50 conversation scenarios
        |
        v
composite_score  →  keep/discard  →  repeat
```

## References

- [Karpathy's announcement on X](https://x.com/karpathy/status/2030371219518931079)
- [program.md source](https://github.com/karpathy/autoresearch/blob/master/program.md)
- [analysis.ipynb](https://github.com/karpathy/autoresearch/blob/master/analysis.ipynb)
