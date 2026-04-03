#!/usr/bin/env bash
# Verify that key docs have consistent, up-to-date numeric claims.
# Compares counts from repo-metrics.sh against claims in CLAUDE.md,
# AGENTS.md, ARCHITECTURE.md, and README.md.
# Fails if any count is off by more than a tolerance margin.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

EXIT_CODE=0
DRIFT_COUNT=0

eval "$(bash scripts/repo-metrics.sh)"

check_metric() {
  local file="$1"
  local label="$2"
  local actual="$3"
  local pattern="$4"
  local tolerance="${5:-10}"

  if [ ! -f "$file" ]; then return; fi

  while IFS= read -r line; do
    claimed=$(echo "$line" | grep -oE "$pattern" | grep -oE '[0-9,]+' | head -1 | tr -d ',')
    if [ -z "$claimed" ] || [ "$claimed" -eq 0 ] 2>/dev/null; then continue; fi

    diff=$((actual - claimed))
    if [ "$diff" -lt 0 ]; then diff=$((-diff)); fi

    pct=0
    if [ "$actual" -gt 0 ]; then
      pct=$((diff * 100 / actual))
    fi

    if [ "$pct" -gt "$tolerance" ]; then
      echo "  DRIFT: $file $label: claimed $claimed, actual $actual (${pct}% off)"
      DRIFT_COUNT=$((DRIFT_COUNT + 1))
      EXIT_CODE=1
      return
    fi
  done < <(grep -E "$pattern" "$file" 2>/dev/null || true)
}

echo "Checking metric consistency across docs..."
echo ""

KEY_DOCS=("CLAUDE.md" "AGENTS.md" "ARCHITECTURE.md" "README.md")

for doc in "${KEY_DOCS[@]}"; do
  if [ ! -f "$doc" ]; then continue; fi
  echo "  Checking $doc..."
  check_metric "$doc" "test_cases" "$TEST_CASES" '[0-9,]+\+? tests[^/]' 15
  check_metric "$doc" "test_files" "$TEST_FILES" '[0-9,]+ test files' 15
  check_metric "$doc" "channels"   "$CHANNEL_ENUM" '[0-9]+ (messaging )?channels' 15
  check_metric "$doc" "tools"      87              '[0-9]+ tool impl' 15
  check_metric "$doc" "fuzz"       "$FUZZ_HARNESSES" '[0-9]+ libFuzzer' 15
done

echo ""
if [ "$DRIFT_COUNT" -eq 0 ]; then
  echo "  No metric drift detected."
else
  echo "  Found $DRIFT_COUNT metric drift(s)."
  echo "  Run 'bash scripts/repo-metrics.sh --human' to see current values."
fi

exit $EXIT_CODE
