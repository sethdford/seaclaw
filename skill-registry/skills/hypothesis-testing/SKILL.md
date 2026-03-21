# Hypothesis Testing

Treat beliefs as provisional. Make predictions that could fail, run the cheapest honest test, and update explicitly when data arrives—especially disconfirming data.

## When to Use
- Product bets, performance claims, root-cause theories, or decisions where intuition and narrative outrun evidence.

## Workflow
1. State the question and the current belief (prior) in plain language.
2. Form a specific, falsifiable hypothesis (“If X, then we will observe Y under conditions Z”).
3. Define in advance what would prove you wrong (pre-register success/failure signals).
4. Design the smallest experiment that resolves ambiguity (prototype, A/B, benchmark, spike, user session).
5. Collect data without moving the goalposts; log methodology limits.
6. Update the belief (Bayesian mindset: prior + likelihood of evidence); note residual uncertainty.
7. Document what was learned and the next test if still ambiguous.

## Examples
**Example 1:** “Users want feature F”: hypothesis—if we ship a stub behind a flag, >30% of trials complete core task; otherwise pivot framing.

**Example 2:** “DB is the bottleneck”: predict p95 under controlled query set; if unchanged after index change, reject that theory and instrument elsewhere.
