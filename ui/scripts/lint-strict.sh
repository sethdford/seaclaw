#!/usr/bin/env bash
# lint-strict.sh — check for quality regressions beyond standard lint
set -euo pipefail

cd "$(dirname "$0")/.."
FAIL=0

# Check for TypeScript `any` in production code
ANY_COUNT=$(rg ': any\b|as any\b' src/ --type ts -g '!*.test.ts' -c 2>/dev/null | awk -F: '{s+=$2} END {print s+0}')
if [ "$ANY_COUNT" -gt 0 ]; then
  echo "FAIL: Found $ANY_COUNT TypeScript 'any' usages in production code:"
  rg ': any\b|as any\b' src/ --type ts -g '!*.test.ts' 2>/dev/null
  FAIL=1
fi

# Check for console statements in production code
CONSOLE_COUNT=$(rg 'console\.(log|warn|error|debug)' src/ --type ts -g '!*.test.ts' -c 2>/dev/null | awk -F: '{s+=$2} END {print s+0}')
if [ "$CONSOLE_COUNT" -gt 0 ]; then
  echo "FAIL: Found $CONSOLE_COUNT console statements in production code:"
  rg 'console\.(log|warn|error|debug)' src/ --type ts -g '!*.test.ts' 2>/dev/null
  FAIL=1
fi

# Report unused component count (informational, not blocking)
TOTAL=$(ls src/components/sc-*.ts 2>/dev/null | grep -v test | wc -l | tr -d ' ')
USED=0
for f in src/components/sc-*.ts; do
  [ -f "$f" ] || continue
  name=$(basename "$f" .ts)
  refs=$(rg -l "$name" src/ --type ts -g '!*.test.ts' -g '!src/components/*.ts' 2>/dev/null | wc -l | tr -d ' ')
  if [ "$refs" -gt 0 ]; then
    USED=$((USED + 1))
  fi
done
UNUSED=$((TOTAL - USED))
echo "INFO: Components: $TOTAL total, $USED used in views, $UNUSED library-only"

# Report unused token count (informational)
DEFINED=$(rg '^\s*--sc-[a-z]' src/styles/_tokens.css -o 2>/dev/null | sed 's/://' | sort -u | wc -l | tr -d ' ')
REFERENCED=$(rg 'var\(--sc-' src/ --type ts --type css -o 2>/dev/null | sed 's/var(//;s/[,)].*//' | sort -u | wc -l | tr -d ' ')
echo "INFO: CSS tokens: $DEFINED defined, $REFERENCED referenced in UI"

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi

echo "lint:strict passed"
