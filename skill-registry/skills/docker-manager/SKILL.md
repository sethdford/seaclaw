---
name: docker-manager
description: Manage Docker containers, images, and compose stacks
---

# Docker Manager

Operate containers and Compose stacks predictably. Pin image digests in production; avoid `latest` for deployables.

Keep volumes, secrets, and networks explicit; document rebuild vs migrate steps.

## When to Use
- Local dev parity, CI images, or production orchestration prep

## Workflow
1. Inspect `Dockerfile` and compose files for user, healthcheck, resource limits.
2. Build multi-stage images; scan for vulnerabilities in base layers.
3. Use `.dockerignore`; mount secrets via runtime, not layers.
4. Prune unused images safely; backup volumes before destructive ops.

## Examples
**Example 1:** Debug container: `exec` in with same env; reproduce with minimal compose override.

**Example 2:** Compose profiles for optional services (observability stack).
