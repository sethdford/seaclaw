---
name: log-analyzer
description: Parse and analyze application and system logs
---

# Log Analyzer

Extract signal from noisy logs. Correlate by request id, trace id, or timestamp windows; distinguish client errors from server defects.

Sampling and aggregation help at high volume—full grep is not always feasible.

## When to Use
- Post-incident review, flaky test diagnosis, or performance regressions

## Workflow
1. Identify log format (JSON vs plain); pick time range and environment.
2. Filter by level, service, version; top-N error messages.
3. Reconstruct failing sequences; link to deploy graph if errors spike after release.
4. Propose monitors/alerts on durable error signatures.

## Examples
**Example 1:** Spike in `timeout` after dependency upgrade: diff latency percentiles.

**Example 2:** Single user errors: trace `user_id` if policy allows and PII is masked.
