---
name: plugin-scanner
description: Scan and list installed skills and plugins
---

# Plugin Scanner

Inventory installed skills and plugins with versions and sources. Detect duplicates, shadowed paths, and disabled entries that still load.

Map registry metadata to disk layout when present.

## When to Use
- Debugging “skill not found”, auditing installs, or preparing support bundles

## Workflow
1. Scan configured plugin directories and environment overrides.
2. Parse manifests (`*.skill.json`, `SKILL.md` frontmatter) for name/version/description.
3. Flag conflicts: same name, different versions on path precedence.
4. Output sorted table: name, version, path, enabled, dependencies.

## Examples
**Example 1:** Two copies of `email-digest`—show which wins per load order rules.

**Example 2:** Orphan skill on disk not referenced in registry—suggest cleanup or registration.
