---
name: config-validator
description: Validate Human and skill configuration files
---

# Config Validator

Validate Human and skill configuration for schema, types, and unsafe combinations. Fail with actionable paths; suggest fixes, don’t silently coerce.

Cross-check referenced files and plugin names exist.

## When to Use
- CI for dotfiles, preflight before deploy, or debugging mysterious defaults

## Workflow
1. Load JSON/YAML/TOML per project conventions; parse strictly.
2. Validate against schema or code-defined rules; collect all violations.
3. Check paths, URLs (HTTPS-only where policy requires), and dependency pins.
4. Emit machine-readable report for CI annotations.

## Examples
**Example 1:** Unknown channel key—did you mean `telegram`? Fuzzy match with edit distance.

**Example 2:** Dangerous tool enabled without sandbox policy—block with error text.
