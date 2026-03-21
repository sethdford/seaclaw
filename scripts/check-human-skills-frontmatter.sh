#!/usr/bin/env bash
# Require YAML frontmatter (---) on line 1 for human-skills/*.md index docs only.
# Per-skill SKILL.md under ~/.human/skills may omit frontmatter; this tree's top-level guides do not.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "Checking human-skills/*.md YAML frontmatter..."
echo ""

EXIT_CODE=0
missing=0
shopt -s nullglob
for f in human-skills/*.md; do
  if ! head -1 "$f" | grep -q '^---$'; then
    echo "  MISSING: $f"
    missing=$((missing + 1))
    EXIT_CODE=1
  fi
done
shopt -u nullglob

if [ "$missing" -eq 0 ]; then
  echo "  All human-skills index Markdown files have frontmatter."
fi

exit "$EXIT_CODE"
