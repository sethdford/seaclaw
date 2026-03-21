#!/usr/bin/env bash
# Require YAML frontmatter (---) on line 1 for Markdown under docs/.
# Default: same as CI — skip docs/plans/ (historical plans vary).
# Set DOC_FLEET_STRICT=1 to require frontmatter in docs/plans/ too.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

EXIT_CODE=0
echo "Checking docs/ YAML frontmatter (line 1 must be ---)..."
echo ""

find_expr=(docs -name '*.md')
if [ "${DOC_FLEET_STRICT:-0}" != "1" ]; then
  find_expr+=(-not -path 'docs/plans/*')
fi

missing=0
while IFS= read -r -d '' f; do
  if ! head -1 "$f" | grep -q '^---$'; then
    echo "  MISSING: $f"
    missing=$((missing + 1))
    EXIT_CODE=1
  fi
done < <(find "${find_expr[@]}" -print0 | sort -z)

if [ "$missing" -eq 0 ]; then
  echo "  All scanned docs have frontmatter."
else
  echo ""
  echo "Found $missing file(s) without frontmatter on line 1."
  if [ "${DOC_FLEET_STRICT:-0}" != "1" ]; then
    echo "  (docs/plans/ skipped; set DOC_FLEET_STRICT=1 to include plans)"
  fi
fi

exit "$EXIT_CODE"
