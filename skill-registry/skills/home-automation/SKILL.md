---
name: home-automation
description: Control smart home devices and automation rules
---

# Home Automation

Automate the home safely: explicit triggers, debouncing, and failure modes. Physical safety (locks, heat, garage) demands confirmation and audit logs.

Respect household privacy and local-first options when available.

## When to Use
- Scenes, schedules, sensor rules, or integrating new devices

## Workflow
1. Inventory hubs, protocols (Zigbee/Matter/Wi-Fi), and device capabilities.
2. Define automations as: trigger → conditions → actions; avoid race conditions.
3. Test with manual runs; add notifications for unexpected state changes.
4. Document voice command aliases and guest access boundaries.

## Examples
**Example 1:** Motion night lights: brightness cap, timeout, don’t trigger during away mode.

**Example 2:** Away mode: close blinds, set thermostat bounds, arm cameras per policy.
