---
name: claw-migrator
description: Migrate configs and skills between Human instances
---

# Claw Migrator

Move configs and skills between Human instances without drift. Treat secrets separately from non-secret config; rotate after migration.

Verify target version compatibility; feature flags may differ between environments.

## When to Use
- Laptop → server moves, staging → prod promotion of skills, or disaster recovery

## Workflow
1. Inventory source: config paths, skills, channels, credentials references.
2. Export non-secrets to structured files; use secure channel for secrets (never slack/email).
3. Apply on target with dry-run validation; run health checks.
4. Document manual steps and rollback: keep old instance read-only until verified.

## Examples
**Example 1:** Copy `~/.human` skill defs but inject env-specific API base URLs.

**Example 2:** Diff registry versions; pin incompatible skills until runtime upgraded.
