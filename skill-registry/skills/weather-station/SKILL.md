---
name: weather-station
description: Aggregate and display weather data from sensors
---

# Weather Station

Aggregate sensor readings with calibration and gap handling. Note units, elevation, and siting effects; label provisional data when sensors drift.

Expose trends (pressure, temperature) useful for local forecasting heuristics.

## When to Use
- Personal weather dashboards, garden automation, or station health checks

## Workflow
1. Validate timestamps and time zones; detect stuck or out-of-range values.
2. Calibrate against reference instruments periodically.
3. Smooth noisy data thoughtfully; preserve raw logs for debugging.
4. Alert on offline sensors or battery low.

## Examples
**Example 1:** Rain gauge: debounce tipping bucket spikes; daily totals vs rate.

**Example 2:** Wind: gust vs average; mount height correction notes in UI.
