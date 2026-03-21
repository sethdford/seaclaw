# Prototyping

Optimize for **learning**, not shipping the prototype. Match fidelity to the single question under test; time-box in hours or days; discard artifacts once the hypothesis is answered.

## When to Use
- Uncertainty is high on UX, feasibility, performance, or integration risk.
- Stakeholders confuse “demo” with “commitment.”
- You need evidence before a large build or refactor.

## Workflow
1. Write **one** falsifiable question (e.g., “Will users complete setup without docs?”).
2. Choose fidelity: paper sketch → wireframe → clickable mock → thin code spike → broader MVP—only as high as the question demands.
3. Time-box ruthlessly; scope the prototype to exercise **only** the risk.
4. Run the test, log pass/fail and surprises, then **throw away** or archive—do not let the spike become accidental production.

## Examples
**Example 1:** API doubt: spike the slowest external call path in isolation before designing caching everywhere.

**Example 2:** Layout risk: one clickable screen for the critical path before building the full component library.
