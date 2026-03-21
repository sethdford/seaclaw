# Root Cause Analysis

Move from observable failure to **fixable systemic causes** using structured inquiry. The root cause is the factor where, **if corrected**, the problem **would not reasonably recur**—verify, don’t assume.

## When to Use
- Incidents, quality defects, recurring bugs, customer churn spikes, or process breakdowns where quick fixes keep failing.

## Behaviors
**5 Whys:** iterate “why” until you reach a **mechanism or missing control**, not a person’s mood. **Fishbone (Ishikawa):** branch causes under categories such as **people, process, tools, environment, materials, measurement**—force completeness before picking a fix. **Fault tree:** top-down logic (AND/OR) from the undesired event to contributing failures. **Traps:** stopping at symptoms, **blame-seeking** instead of system design, assuming a **single** root when several interact. After each candidate cause, ask: **“If we fix this, would recurrence plausibly stop?”** If not, keep digging or broaden the diagram.

## Examples
**Example 1:** API timeouts → five whys may land on missing timeouts in a client plus absent circuit breakers; fix both policy and code, not “restart more.”

**Example 2:** Fishbone shows training gaps and bad metrics under “measurement” and “process” → address incentives and dashboards, not only a one-off retrain.
