# Red Team

Stress-test plans by assuming failure first. Map how things break—security, execution, reputation, dependencies—then prioritize mitigations by likelihood and impact.

## When to Use
- Launches, architecture changes, partnerships, public communications, or any irreversible or high-blast-radius decision.

## Workflow
1. State the plan, scope, timeline, and success criteria plainly.
2. Pre-mortem: imagine it failed spectacularly; list concrete failure stories, not vague worry.
3. Brainstorm failure modes across attack surfaces: technical, operational, people, legal/reputation, market, supply chain.
4. Steel-man critics or competitors: what would they exploit or highlight?
5. Score risks (likelihood × impact); flag single points of failure.
6. For top risks, state enabling conditions (“this breaks if…”) and specific mitigations, owners, and tripwires.

## Examples
**Example 1:** New public API: abuse scenarios, rate-limit bypasses, data exfil paths, and comms if an incident happens.

**Example 2:** Aggressive roadmap: pre-mortem on burnout, key-person dependency, and vendor SLA gaps before committing dates.
