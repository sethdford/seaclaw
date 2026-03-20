---
name: ci-cd-helper
description: Configure CI/CD pipelines and deployment automation
---

# Ci Cd Helper

Design pipelines that give fast feedback and reliable deploys. Fail fast on lint/test; use caches; separate build, test, and deploy stages with clear artifacts.

Secrets belong in the CI secret store, never in logs or repo files.

## When to Use
- Setting up GitHub Actions/GitLab CI/Jenkins, or hardening existing pipelines

## Workflow
1. List required jobs: build, unit tests, integration (optional), security scan, deploy.
2. Match runner images to project toolchains; pin action versions or digests.
3. Gate deploy on main branch + tags; use environments and approvals for production.
4. Add rollback strategy and health checks post-deploy.

## Examples
**Example 1:** Matrix build across OS versions for a C library with native code.

**Example 2:** Canary deploy: 5% traffic, automated rollback on error rate spike.
