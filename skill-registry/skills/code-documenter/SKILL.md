---
name: code-documenter
description: Generate docs, READMEs, and API documentation from code
---

# Code Documenter

Produce documentation that helps the next maintainer act without reading the whole codebase. Document invariants, failure modes, and extension points—not obvious syntax.

Keep READMEs scoped to quickstart, architecture links, and troubleshooting; generate API docs from annotations or comments where tools exist.

## When to Use
- New packages, public APIs, complex algorithms, or operational runbooks

## Workflow
1. Identify audience (user vs contributor vs operator) and doc surface (README, `docs/`, docstrings).
2. Summarize purpose, install, config, and “how to run tests” in README.
3. For APIs: parameters, returns, errors, and examples; link to schemas.
4. Update changelog or migration notes when behavior changes.

## Examples
**Example 1:** Library: module-level doc with minimal example and thread-safety notes.

**Example 2:** CLI: `--help` text plus `docs/cli.md` for scripting examples.
