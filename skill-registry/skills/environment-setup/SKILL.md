---
name: environment-setup
description: Setup development and deployment environments
---

# Environment Setup

Bootstrap dev and deploy environments reproducibly. Pin tool versions; document OS differences; one-command bootstrap when possible.

Prefer container or devcontainer for parity; keep native paths documented for contributors.

## When to Use
- New machine onboarding, CI image updates, or “works on my machine” fixes

## Workflow
1. List required runtimes, compilers, package managers, and env vars.
2. Provide install steps per OS or use infra-as-code (Docker, Nix, mise).
3. Seed secrets via template `.env.example` without real values.
4. Verify: build, test, lint commands all succeed from clean checkout.

## Examples
**Example 1:** Document `cmake --preset dev` + Homebrew packages for macOS vs apt on Linux.

**Example 2:** Devcontainer: extensions, port forwards, and post-create script for hooks.
