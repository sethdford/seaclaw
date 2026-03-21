#!/usr/bin/env bash
# Validate relative file targets in docs/**/*.md (see check_docs_relative_links.py).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec python3 "$SCRIPT_DIR/check_docs_relative_links.py"
