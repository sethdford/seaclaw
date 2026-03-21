# Probabilistic Thinking

Reason under uncertainty with **explicit degrees of belief**, **ranges**, and **trade-offs**—not false certainty. Use simple **expected value** and **tail awareness** to compare options.

## When to Use
- Forecasting, prioritization, risk reviews, investment or roadmap choices, or when stakeholders demand single-point answers.
- When a decision hinges on rare events (“black swans”) that averages and dashboards hide.

## Behaviors
Prefer **ranges** and **confidence language** (“roughly 60–80% likely”) over absolutes. **Expected value** ≈ **probability × payoff** (a 10% shot at $1M has ~$100K expectation—compare to alternatives). Look at the **distribution**: means hide **tail risks** and ruin scenarios. Run a **pre-mortem**: list failure modes, assign **rough probabilities**, and design mitigations for the heavy tails. **Update** as evidence arrives (**Bayesian** intuition: shift beliefs proportionally to how surprising the data is). Separate **risk** (known-ish probabilities) from **uncertainty** (opaque odds)—choose learning, buffers, or options accordingly.

## Examples
**Example 1:** Two features: A is 90% likely +2% conversion; B is 40% likely +10% → sketch EV and downside if B fails (schedule slip) before deciding.

**Example 2:** “It won’t happen” for a rare outage → assign even a small probability; if cost is huge, invest in detection and rollback, not optimism.
