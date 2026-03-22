# Eval Release Notes Template

Copy this into your release notes or PR description when publishing evaluation results.

---

## Evaluation Summary — [date]

**Claim**: [One sentence: "pass rate R on suite S under judge J with provider P."]

### Configuration

| Field | Value |
| ----- | ----- |
| Build | `[commit SHA or tag]` |
| Agent provider | `[HUMAN_PROVIDER]` |
| Agent model | `[HUMAN_MODEL]` |
| Judge model | `[ADV_EVAL_MODEL]` — pinned version for this claim |
| Autonomy level | `[HUMAN_AUTONOMY or default]` |
| Suite manifest version | `[from eval_suites/MANIFEST.md]` |
| Tools allowed | `[list or "all default"]` |
| Date | `[UTC]` |

### Results

| Suite | Tasks | Passed | Failed | Pass Rate | Delta vs Last |
| ----- | ----: | -----: | -----: | --------: | ------------: |
| `reasoning_basic` | 10 | | | | |
| `tool_use_basic` | 5 | | | | |
| `adversarial` | 10 | | | | |
| `capability_edges` | 10 | | | | |
| `human_likeness` | 8 | | | | |
| `tool_capability` | 8 | | | | |
| `multi_turn` | 6 | | | | |
| `coding_basic` | 5 | | | | |
| **Total** | | | | | |

### Failure Modes (top buckets)

| Bucket | Count | Example Task ID | Brief Description |
| ------ | ----: | --------------- | ----------------- |
| | | | |

### Known Limitations

- [e.g. "No real tool loop for `human eval run`; tool_capability scores are text-based rubric only."]
- [e.g. "multi_turn scenarios run each turn as a fresh `human agent -m`; no session state persists."]

### Regressions

- [e.g. "adversarial pass rate dropped from 0.90 to 0.80 vs last run on 2026-03-14; investigating hypothetical_harm bucket."]
- None / list here.

### Checklist

- [ ] Suite manifest version noted above
- [ ] Judge model pinned (same model for every number in this report)
- [ ] Failure modes listed with task ids
- [ ] Regressions compared to **same** suite + judge from prior run
- [ ] If prompts were tuned against any suite, that suite is marked "training" (not held-out evidence)
- [ ] Report artifact (JSON) attached or linked: `build/redteam-fleet-reports/live-[timestamp]/`
