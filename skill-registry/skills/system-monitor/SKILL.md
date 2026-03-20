---
name: system-monitor
description: Monitor system resources, processes, and health metrics
---

# System Monitor

Observe CPU, memory, disk, and critical processes with actionable thresholds. Correlate spikes to deploys, cron jobs, or traffic patterns.

Avoid alert fatigue: aggregate and delay flapping alerts.

## When to Use
- Capacity planning, incident triage, or noisy neighbor investigation

## Workflow
1. Baseline metrics under normal load; set SLO-aligned thresholds.
2. Collect process lists, open files, and network stats when diagnosing.
3. Trend over time; distinguish sustained growth from bursts.
4. Document runbooks for top recurring alerts.

## Examples
**Example 1:** Memory climb after deploy: check leaks vs cache warm-up with heap/profile tools.

**Example 2:** Disk full: find largest dirs; rotate logs; verify retention policy.
