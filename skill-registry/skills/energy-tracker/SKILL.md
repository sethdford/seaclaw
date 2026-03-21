---
name: energy-tracker
description: Track energy consumption from smart meters
---

# Energy Tracker

Relate smart-meter data to appliances and time-of-use tariffs. Normalize kWh intervals; align with billing cycles for cost accuracy.

Privacy: aggregate or anonymize when sharing outside the household.

## When to Use
- Bill anomaly investigation, solar/battery sizing, or efficiency projects

## Workflow
1. Ingest interval data; handle DST and missing reads.
2. Attribute major loads (HVAC, EV) using signatures or submeters if available.
3. Compare to tariff windows; suggest shiftable load schedules.
4. Validate totals against utility invoice.

## Examples
**Example 1:** Spike at 2am: identify pool pump schedule vs rate window.

**Example 2:** Solar export: net metering vs NEM 3 rules in summaries.
