---
name: dependency-checker
description: Check and update project dependencies for security and compatibility
---

# Dependency Checker

Evaluate dependencies for security advisories, license compatibility, and upgrade risk. Prefer evidence from lockfiles and official advisories over guesswork.

Pin versions in applications; libraries may semver-range per ecosystem norms. Document breaking changes when upgrading majors.

## When to Use
- Periodic audits, CVE reports, or before a release freeze

## Workflow
1. Identify manifest and lockfile (`package-lock.json`, `Cargo.lock`, `go.sum`, etc.).
2. Run ecosystem audit tools (`npm audit`, `cargo audit`, OSV) and triage: exploitable vs theoretical.
3. Patch minors/patches first; schedule majors with a changelog review and test plan.
4. Record decisions for accepted risk (with expiry) when not upgrading immediately.

## Examples
**Example 1:** High severity in transitive dep: update direct parent or use override/resolution per package manager docs.

**Example 2:** Unmaintained package: evaluate replacement vs fork vs vendor with legal review.
