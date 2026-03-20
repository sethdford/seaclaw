---
name: uptime-checker
description: Monitor endpoint uptime and availability
---

# Uptime Checker

Measure availability and latency from probes that match user geography. Distinguish total outage vs elevated latency; avoid false greens from cached error pages.

Synthetic checks complement real user monitoring—they do not replace it.

## When to Use
- SLA reporting, on-call tuning, or validating new regions/CDNs

## Workflow
1. Define critical endpoints and expected status/body checks.
2. Run probes from multiple locations; store history for trend graphs.
3. Page on sustained failure; ticket on flaky patterns.
4. Postmortem: include timeline, blast radius, and fix verification.

## Examples
**Example 1:** Health URL must return 200 and JSON `"status":"ok"` within 500ms p95.

**Example 2:** Auth endpoints: use synthetic login with test credentials in isolated env.
