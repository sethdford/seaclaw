#!/usr/bin/env bash
# Cross-surface token consistency check.
# Compares design token usage between dashboard (ui/) and website (website/)
# to detect drift where one surface uses raw values or different tokens.
# Usage: scripts/check-token-consistency.sh
set -euo pipefail

cd "$(dirname "$0")/.."

FAILURES=0

echo "=== Cross-Surface Token Consistency Check ==="
echo ""

# 1. Check for raw hex colors in both surfaces
echo "--- Raw hex colors ---"
for surface in ui/src website/src; do
  if [ ! -d "$surface" ]; then continue; fi
  count=$(rg -c '#[0-9a-fA-F]{3,8}' --type ts --type css "$surface" 2>/dev/null | awk -F: '{s+=$2}END{print s+0}')
  if [ "$count" -gt 0 ]; then
    echo "WARNING: $surface has $count raw hex color references"
    FAILURES=$((FAILURES + 1))
  else
    echo "PASS: $surface — no raw hex colors"
  fi
done

# 2. Check for raw px values (spacing/radius) — exclude font-size, line-height, and standard resets
echo ""
echo "--- Raw px spacing/radius ---"
for surface in ui/src website/src; do
  if [ ! -d "$surface" ]; then continue; fi
  count=$(rg -c ':\s*\d+px' --type ts --type css "$surface" 2>/dev/null | awk -F: '{s+=$2}END{print s+0}')
  if [ "$count" -gt 20 ]; then
    echo "WARNING: $surface has $count raw px declarations (review for token drift)"
  else
    echo "PASS: $surface — $count raw px (within tolerance)"
  fi
done

# 3. Check for Google Fonts imports
echo ""
echo "--- External font imports ---"
for surface in ui/src website/src; do
  if [ ! -d "$surface" ]; then continue; fi
  count=$(rg -c 'fonts.googleapis.com|font-family\s*:(?!.*var\()' --type ts --type css "$surface" 2>/dev/null | awk -F: '{s+=$2}END{print s+0}')
  if [ "$count" -gt 0 ]; then
    echo "FAIL: $surface has $count non-token font references"
    FAILURES=$((FAILURES + 1))
  else
    echo "PASS: $surface — all fonts via tokens"
  fi
done

# 4. Check for rgba() usage (should use color-mix)
echo ""
echo "--- rgba() usage (should use color-mix) ---"
for surface in ui/src website/src; do
  if [ ! -d "$surface" ]; then continue; fi
  count=$(rg -c 'rgba\(' --type ts --type css "$surface" 2>/dev/null | awk -F: '{s+=$2}END{print s+0}')
  if [ "$count" -gt 0 ]; then
    echo "WARNING: $surface has $count rgba() calls — prefer color-mix()"
  else
    echo "PASS: $surface — no rgba()"
  fi
done

# 5. Check for hardcoded durations (should use tokens)
echo ""
echo "--- Hardcoded animation durations ---"
for surface in ui/src website/src; do
  if [ ! -d "$surface" ]; then continue; fi
  count=$(rg -c '\d+ms(?!.*var)' --type ts --type css "$surface" 2>/dev/null | awk -F: '{s+=$2}END{print s+0}')
  if [ "$count" -gt 5 ]; then
    echo "WARNING: $surface has $count hardcoded duration values"
  else
    echo "PASS: $surface — $count hardcoded durations (within tolerance)"
  fi
done

echo ""
if [ "$FAILURES" -gt 0 ]; then
  echo "RESULT: $FAILURES consistency issue(s) found"
  exit 1
else
  echo "RESULT: All cross-surface checks passed"
fi
