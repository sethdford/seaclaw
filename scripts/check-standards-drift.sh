#!/usr/bin/env bash
# Verify that CLAUDE.md and AGENTS.md reference docs/standards/ paths correctly
# and that no stale references to old doc locations remain.
set -euo pipefail

EXIT_CODE=0

echo "Checking for stale doc references..."
echo ""

stale_patterns=(
  'docs/visual-standards\.md'
  'docs/motion-design\.md'
  'docs/ux-patterns\.md'
  'docs/design-strategy\.md'
  'docs/design-system\.md'
  'docs/security/threat-model\.md'
  'docs/security/sandbox\.md'
)

files_to_check=(
  "CLAUDE.md"
  "AGENTS.md"
  ".cursor/rules/design-system.mdc"
  ".cursor/rules/design-tokens.mdc"
  ".cursor/rules/view-archetypes.mdc"
  ".cursor/rules/animation.mdc"
)

stale_count=0
for pattern in "${stale_patterns[@]}"; do
  for file in "${files_to_check[@]}"; do
    if [ -f "$file" ] && grep -qE "$pattern" "$file" 2>/dev/null; then
      match=$(grep -nE "$pattern" "$file" | head -1)
      echo "  STALE: $file: $match"
      echo "         Should reference docs/standards/ path instead"
      stale_count=$((stale_count + 1))
      EXIT_CODE=1
    fi
  done
done

if [ "$stale_count" -eq 0 ]; then
  echo "  No stale references found."
fi

echo ""
echo "Checking standards directory completeness..."

expected_dirs=("ai" "design" "engineering" "operations" "quality" "security")
missing=0
for dir in "${expected_dirs[@]}"; do
  if [ ! -d "docs/standards/$dir" ]; then
    echo "  MISSING: docs/standards/$dir/"
    missing=$((missing + 1))
    EXIT_CODE=1
  else
    count=$(find "docs/standards/$dir" -name "*.md" | wc -l | tr -d ' ')
    echo "  OK: docs/standards/$dir/ ($count files)"
  fi
done

if [ "$missing" -gt 0 ]; then
  echo ""
  echo "Found $missing missing standards directory(ies)."
fi

echo ""
echo "Checking README.md index exists..."
if [ -f "docs/standards/README.md" ]; then
  echo "  OK: docs/standards/README.md exists"
else
  echo "  MISSING: docs/standards/README.md"
  EXIT_CODE=1
fi

exit $EXIT_CODE
