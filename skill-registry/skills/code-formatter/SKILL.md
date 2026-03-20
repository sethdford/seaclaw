---
name: code-formatter
description: Format and lint code according to project conventions
---

# Code Formatter

Apply formatting and linting consistent with project configuration. Do not impose personal style where the repo already defines clang-format, eslint, black, etc.

If tooling is missing, propose minimal config that matches existing code style—avoid drive-by reformat of unrelated files.

## When to Use
- Pre-commit cleanup, CI failures on style, or normalizing new code to repo conventions

## Workflow
1. Detect authoritative config (`.editorconfig`, `pyproject.toml`, `.clang-format`, `package.json` scripts).
2. Run the project’s canonical format command; if none, format only touched lines or files.
3. Separate formatting commits from logic commits when history clarity matters.
4. Re-run tests after mass formatting.

## Examples
**Example 1:** Fix CI: run `npm run format` or equivalent, commit as `style: apply formatter`.

**Example 2:** Single file: apply same indentation and quote style as neighboring files in the directory.
