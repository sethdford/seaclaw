#!/usr/bin/env bash
# Validate relative file targets in Markdown (see check_markdown_relative_links.py).
# Default roots: docs/, human-skills/, skill-registry/, plus top-level *.md.
# MARKDOWN_LINK_SCAN_ALL=1 — entire repo except junk dirs (.git, node_modules, build, …).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec python3 "$SCRIPT_DIR/check_markdown_relative_links.py"
