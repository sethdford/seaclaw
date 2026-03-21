# Risk Assessment

Make uncertainty legible before it becomes a surprise. Separate known risks, known unknowns, and unknown unknowns; choose mitigate, accept, avoid, or transfer with clear triggers.

## When to Use
- Launching changes, signing commitments, or operating under tight SLAs.
- Decisions where failure modes are plausible but not yet quantified.

## Workflow
1. **List risks** by source (technical, people, vendor, legal, security, reputation); include “silent” dependencies.
2. **Probability × impact**: use coarse bands (e.g., low/med/high) if data is thin; note confidence in each rating.
3. **Plot** on a matrix; highlight top-right cluster as the review set.
4. **Scenarios**: sketch best / base / worst; name drivers that flip between them.
5. **Pre-mortem**: “It’s six months from now and we failed—why?” generate failure stories, then back-map preventions.
6. **Black swan probe**: where would a single external shock dominate? Avoid false precision; flag monitoring needs.
7. **Strategies per risk**: mitigate (reduce P or I), accept (explicit owner), avoid (change approach), transfer (insurance, contract).
8. **Escalation triggers**: define observable signals and who decides when to pivot or stop.
9. **Risk appetite**: align tactics with how much downside the team/org can absorb this quarter.

## Examples
**Example 1:** New integration: probability/impact on auth outages; pre-mortem on credential rotation; mitigation runbooks + on-call threshold.

**Example 2:** Vendor dependency: scenario on vendor exit; contract and data portability as mitigation; executive trigger if SLA breaches twice.
