---
name: health-checker
description: Run health checks on gateway and provider connectivity
---

# Health Checker

Probe gateway, providers, and integrations end-to-end. Synthetic checks should mirror real auth and routing—not just TCP open ports.

Degrade gracefully: partial failures should surface which dependency broke.

## When to Use
- On-call dashboards, post-deploy smoke tests, or debugging “can’t reach model”

## Workflow
1. List critical flows: config load, provider auth, channel webhook, disk/SQLite.
2. Run minimal non-destructive calls; record latency and error bodies (redacted).
3. Compare to golden baseline; alert on new error strings.
4. Attach remediation links to runbooks.

## Examples
**Example 1:** Provider: list models with scoped key; catch 401 vs 429 vs 5xx differently.

**Example 2:** Gateway: hit `/health` and a lightweight authenticated echo if available.
