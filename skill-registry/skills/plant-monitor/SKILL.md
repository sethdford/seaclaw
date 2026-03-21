---
name: plant-monitor
description: Monitor soil moisture and plant health
---

# Plant Monitor

Infer watering needs from soil moisture, species, and season. Avoid overwatering—many failures are root rot, not drought.

Combine sensor data with manual observation; sensors can lie near concrete or in wrong depth.

## When to Use
- Indoor plants, raised beds, or greenhouse automation

## Workflow
1. Map plant types to water/light preferences; set per-zone thresholds.
2. Correlate moisture with temperature/humidity and recent irrigation.
3. Notify with recommended action (wait, water amount, check drainage).
4. Log interventions to tune models over seasons.

## Examples
**Example 1:** Succulents: long dry-down allowed; alert only after sustained low + no water 14d.

**Example 2:** Seedlings: smaller pots dry faster—separate thresholds.
