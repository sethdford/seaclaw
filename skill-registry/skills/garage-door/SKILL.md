---
name: garage-door
description: Control garage door opener via smart relay
---

# Garage Door

Control door hardware with safety interlocks and state verification. Never fire open/close without confirming obstruction sensors and motor limits.

Require explicit user confirmation for remote triggers; log all commands.

## When to Use
- Integrating relays, smart openers, or geofence arrival automations

## Workflow
1. Read manufacturer safety docs; comply with local electrical codes.
2. Implement state machine: opening/open/closing/closed with timeouts.
3. Debounce commands; reject duplicate toggles during motion.
4. Alert on stuck states or sensor disagreements.

## Examples
**Example 1:** “Close door” only if camera/obstruction clear per policy.

**Example 2:** Notify if left open >10 minutes after sunset.
