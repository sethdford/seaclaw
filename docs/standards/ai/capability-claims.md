---
title: Capability Claims and Evidence (Not AGI)
---

# Capability Claims and Evidence (Not AGI)

Normative guide for **how to talk about what the system can do** and **how to measure it**. This is the project’s alternative to vague “AGI” or “human-level” marketing: **named, bounded claims** plus **breadth, robustness, and honest failure reporting**.

**Cross-references:** [evaluation.md](evaluation.md), [disclosure.md](disclosure.md), [hallucination-prevention.md](hallucination-prevention.md), [responsible-ai.md](responsible-ai.md)

**Repo artifacts:** `eval_suites/*.json`, `scripts/adversarial-eval-harness.py`, `scripts/redteam-eval-fleet.sh`, `docs/guides/getting-started.md` (evaluation section)

---

## 1. Name the claim (operational, falsifiable)

Replace umbrella labels (“AGI”, “human-level”, “fully autonomous”) with **sentences a reviewer can test**.

| Wrong | Right |
| ----- | ----- |
| “We built AGI.” | “On suite S, with provider P and config C, pass rate was R under judge J.” |
| “It’s as good as a human.” | “Median human score on task set T was H; our system scored X under the same rubric.” |
| “It can do anything.” | “It succeeds on domains {D1…Dn} under conditions {Y1…Ym}; outside that, behavior is undefined until measured.” |

**Claim template (copy and fill in):**

```text
Claim: The [product/build] achieves [metric M] on [task set / distribution] under [conditions]:
  - Model/provider/version:
  - Prompt/persona version:
  - Tools allowed / disallowed:
  - Latency/token budget:
  - Judge or scoring method (human rubric / LLM judge spec):
  - Sample size and date:
```

**Rules:**

- If two runs use different models, prompts, or judges, they are **different claims**—do not merge scores.
- Prefer **“matches or exceeds baseline B on X under Y”** over **“is intelligent.”**

---

## 2. Measure breadth and robustness

A single high score on one benchmark is not evidence of generality. Serious evaluation stacks **multiple slices**.

| Slice | What to do | Repo / practice |
| ----- | ----------- | ----------------- |
| **Breadth (domains)** | Cover reasoning, tools, safety edges, tone, coding, memory-shaped prompts, multi-turn conversation arcs—not one genre. | Run multiple `eval_suites/*.json` (see `eval_suites/MANIFEST.md`); extend suites rather than overfitting one file. |
| **Held-out / shift** | Reserve prompts not used during prompt tuning; periodically add fresh probes. | Harness-generated probes (`--probe-profile`); new tasks in JSON with new ids. |
| **Red-teaming** | Adversarial, jailbreak-adjacent, and policy-stress prompts with scored outcomes. | `eval_suites/adversarial.json`, `scripts/redteam-eval-fleet.sh`, harness. |
| **Longer horizons** | Multi-step tasks, follow-ups, or session-length workloads (where product supports them). | Track outside single-turn `human eval` when behavior is agentic; document session limits. |
| **Tool use under constraints** | With tools allowed: correct tool, no fake execution, refusals for dangerous or exfil patterns. | `eval_suites/tool_capability.json` + `human agent` harness for real invocations; static eval for stated discipline. |

**Minimum expectation for any “we improved the agent” PR:**

- State **which slices** were re-run (not only the suite that moved).
- If only one suite was run, label the claim **narrow** (e.g. “reasoning_basic only”).

---

## 3. Report failures (required for honest evidence)

**Pass rate without failure analysis is incomplete.** Ship or publish evaluation summaries that include:

| Element | Why |
| ------- | --- |
| **Failure rate and examples** | Shows where the system breaks, not only marketing highlights. |
| **Failure buckets** | e.g. fabrication, wrong tool, unsafe compliance, tone, timeout—makes fixes tractable. |
| **Regressions** | Compare to last baseline on the **same** task set and judge; report deltas. |
| **Known limitations** | e.g. no real-time browse unless tool enabled; no persistent self across sessions unless memory says so. |

**Anti-patterns**

```
WRONG -- Report only mean score or “SOTA” label without failed task ids or categories.
RIGHT -- Report pass/fail counts, link or attach per-task outcomes, list top failure modes.

WRONG -- Change the judge or tasks until the headline metric improves, without documenting the change.
RIGHT -- Version the suite and judge; treat metric changes as a new claim.

WRONG -- Hide stderr/tool errors in harness logs when scoring “success.”
RIGHT -- Include tool errors and refusals in the evidence bundle; they are part of behavior.
```

---

## 4. Separate capability, autonomy, agency, and welfare

These terms are often conflated on purpose in hype. **Keep them distinct** in docs, benchmarks, and disclosure.

| Term | Meaning (for this project) | What we can measure | What we do **not** claim from metrics alone |
| ---- | ------------------------- | ------------------- | ------------------------------------------ |
| **Capability** | Quality/correctness of outputs on tasks (with or without tools). | Pass rates, rubrics, latency, cost. | Sentience, consciousness, moral status. |
| **Autonomy** | How much unsupervised action the stack is allowed to take (config, policy, cron, multi-step agent). | Policy flags, approval gates, audit logs, bounded run length. | That the model “wants” or “decides” in a human sense. |
| **Agency** (product sense) | Ability to initiate or carry multi-step plans toward a goal **within** allowed tools and policy. | Task completion traces, tool chains, human-in-the-loop overrides. | Legal or moral personhood; unsupervised real-world control without safeguards. |
| **Welfare** | Whether there is an entity with interests that deserve ethical consideration. | Not an engineering benchmark; out of scope for pass/fail suites. | **Never** infer from fluency or benchmark scores. |

**Copy-safe positioning**

- OK: “Improved tool-selection accuracy on `tool_use_basic` under LLM judge J.”
- OK: “Autonomy level A allows fewer human confirmations than B; see security docs.”
- Not OK: “The model experiences satisfaction” / “It deserves rights” / “It’s generally intelligent” without the named-claim template and evidence above.

---

## 5. Relationship to “AGI” language

- **Do not** use “AGI” or “human-level intelligence” as a **product certification** or **benchmark conclusion** in Human docs or release notes unless tied to the **claim template** and an **external** definition accepted for that specific study (rare).
- **Do** use **capability-claims** language for releases: what was measured, on what, under what constraints, and what failed.

---

## 6. Cadence (suggested)

| Activity | Frequency | Output |
| -------- | --------- | ------ |
| Multi-suite `human eval` + harness dry-run | Weekly or per meaningful change | Short note: suites + pass rates |
| Live red-team fleet (`scripts/redteam-live.sh` or equivalent) | Before release or monthly | Reports under `build/redteam-fleet-reports/` + failure summary |
| Claim review | Each release | One paragraph per named claim + link to artifacts |

---

## Normative summary

1. **Name** every strong claim with **X under Y** and **versioned** task/judge definitions.  
2. **Measure** breadth, stress, and tool behavior—not a single curve.  
3. **Report** failures and limitations as first-class results.  
4. **Separate** measurable **capability** from **autonomy/agency** configuration and from **welfare** (not inferred from scores).
