---
name: git-workflow
description: Git branch strategy, merge workflows, and PR automation
---

# Git Workflow

Align branch naming, merge strategy, and PR hygiene with the team’s conventions and the repository’s documented workflow. Prefer small, reviewable changesets and reversible history.

Avoid rewriting published history without agreement. Keep commit messages meaningful and map them to issues or tickets when the project uses them.

## When to Use
- Planning releases, cleaning up branches, resolving merge conflicts, or automating PR metadata
- Onboarding contributors or standardizing how work flows from issue → branch → review → main

## Workflow
1. Confirm default branch, protected-branch rules, and required checks (CI, reviews).
2. Choose branch type (`feat/`, `fix/`, etc.) per project standards; keep scope to one concern per branch.
3. Rebase or merge based on team policy; document the choice in the PR if non-obvious.
4. Ensure PR description states motivation, risk, test evidence, and rollback notes for risky changes.
5. After merge, delete stale remote branches and verify CI on main.

## Examples
**Example 1:** Hotfix: branch from the release tag, cherry-pick the fix, open PR with `fix:` prefix and link to incident.

**Example 2:** Feature behind flag: one PR for implementation, follow-up for enabling default—avoid mixing refactors with behavior changes.
