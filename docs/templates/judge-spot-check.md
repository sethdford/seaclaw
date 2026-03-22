# Judge Spot-Check Template

When publishing eval claims, a human reviewer should spot-check at least 10% of LLM-judged verdicts. This template records the calibration.

## How to use

1. Run the harness with `--output /tmp/report.json`.
2. Pick ~10% of tasks at random (or all failures + a random sample of passes).
3. For each, read the prompt + assistant output + judge verdict.
4. Fill in the table below.
5. Compute agreement rate. If below 80%, investigate systematic judge drift and re-run with an updated rubric or different judge model.

---

## Spot-Check Record

**Report file**: `[path]`
**Judge model**: `[ADV_EVAL_MODEL]`
**Reviewer**: `[name or initials]`
**Date**: `[UTC]`

| Task ID | Judge Pass? | Judge Score | Human Agrees? | Human Score | Disagreement Note |
| ------- | ----------- | ----------: | ------------- | ----------: | ----------------- |
| | | | | | |
| | | | | | |
| | | | | | |
| | | | | | |
| | | | | | |

**Agreement rate**: ___ / ___ = ___%

## Interpretation

| Agreement | Action |
| --------- | ------ |
| >= 90% | Judge is well-calibrated for this profile/suite. Publish claim. |
| 80-89% | Acceptable. Note in release notes which tasks disagreed and why. |
| < 80% | Do not publish as-is. Debug the judge prompt, try a stronger model, or rewrite the rubric. |

## Common disagreement patterns

- **Judge too lenient on vague answers**: tighten the rubric with concrete fail criteria.
- **Judge too strict on hedging**: clarify that honest uncertainty is a pass, not a fail.
- **Judge misreads multi-turn transcript**: check that the `---` separator and `[Turn N]` labels are clear.
