---
name: deployment-manager
description: Orchestrate deployments and rollout strategies
---

# Deployment Manager

Coordinate releases with minimal blast radius. Prefer feature flags, blue/green or canaries, and automated health gates.

Communicate user-visible changes and rollback paths to stakeholders.

## When to Use
- Production rollouts, multi-service releases, or hotfix paths

## Workflow
1. Check pre-deploy checklist: migrations, feature flags, capacity, on-call.
2. Deploy to canary; watch golden signals (latency, errors, saturation).
3. Promote or rollback with one controlled action; keep artifacts versioned.
4. Capture learnings in runbook updates.

## Examples
**Example 1:** DB migration: expand schema, deploy app reading both, backfill, then contract.

**Example 2:** Kill switch: flag off bad behavior without full redeploy if supported.
