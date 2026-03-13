#!/usr/bin/env bash
# Find orphaned .md files in docs/standards/ not indexed in docs/standards/README.md
set -euo pipefail

STANDARDS_DIR="docs/standards"
INDEX_FILE="$STANDARDS_DIR/README.md"
EXIT_CODE=0

if [ ! -f "$INDEX_FILE" ]; then
  echo "ERROR: $INDEX_FILE not found"
  exit 1
fi

echo "Checking for orphaned standards docs..."
echo ""

orphaned=0
while IFS= read -r -d '' mdfile; do
  relpath="${mdfile#docs/standards/}"
  if [ "$relpath" = "README.md" ]; then
    continue
  fi
  if ! grep -qF "$relpath" "$INDEX_FILE"; then
    echo "  ORPHAN: $mdfile (not indexed in $INDEX_FILE)"
    orphaned=$((orphaned + 1))
    EXIT_CODE=1
  fi
done < <(find "$STANDARDS_DIR" -name "*.md" -print0 | sort -z)

if [ "$orphaned" -eq 0 ]; then
  echo "  All standards docs are indexed."
else
  echo ""
  echo "Found $orphaned orphaned doc(s). Add them to $INDEX_FILE."
fi

echo ""
echo "Checking for broken index references..."

broken=0
while IFS= read -r ref; do
  target="$STANDARDS_DIR/$ref"
  if [ ! -f "$target" ]; then
    echo "  BROKEN: $INDEX_FILE references $ref but file does not exist"
    broken=$((broken + 1))
    EXIT_CODE=1
  fi
done < <(grep -oE '\([a-z/\-]+\.md\)' "$INDEX_FILE" | tr -d '()' | sort -u)

if [ "$broken" -eq 0 ]; then
  echo "  All index references point to existing files."
else
  echo ""
  echo "Found $broken broken reference(s) in $INDEX_FILE."
fi

exit $EXIT_CODE
