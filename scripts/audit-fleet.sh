#!/usr/bin/env bash
# Audit fleet: documentation gates + lightweight static hygiene scans for C.
# Memory leaks: AddressSanitizer in dev/test builds catches heap issues; run
#   cmake --preset dev && cmake --build --preset dev && ./build/human_tests
# Deeper security: CodeQL workflow (.github/workflows/codeql.yml), red-team fleet
#   scripts/redteam-eval-fleet.sh (offline by default).
#
# Usage:
#   bash scripts/audit-fleet.sh
#   DOC_FLEET_LINKS_FAST=1 bash scripts/audit-fleet.sh   # faster doc link scan
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

EXIT_CODE=0

banner() {
  echo ""
  echo "=============================="
  echo " $1"
  echo "=============================="
}

fail() {
  echo "--- FAIL: $1"
  EXIT_CODE=1
}

pass() {
  echo "--- OK: $1"
}

banner "audit-fleet: documentation (doc-fleet.sh)"
if bash scripts/doc-fleet.sh; then
  pass "doc-fleet"
else
  fail "doc-fleet"
fi

banner "audit-fleet: C API hygiene"
scan_c_unsafe() {
  if command -v rg &>/dev/null; then
    rg -n --glob '*.c' "$1" src/ 2>/dev/null || true
  else
    # Portable fallback (BSD grep: no --include; use find)
    find src -name '*.c' -exec grep -n -E "$1" {} + 2>/dev/null || true
  fi
}

# High-risk libc patterns (should stay empty in src/)
if scan_c_unsafe '\b(strcpy|strcat|sprintf|gets)\s*\(' | head -1 | grep -q .; then
  scan_c_unsafe '\b(strcpy|strcat|sprintf|gets)\s*\(' | head -20
  fail "unsafe C string APIs in src/"
else
  pass "no strcpy/strcat/sprintf/gets in src/"
fi

if scan_c_unsafe 'SQLITE_TRANSIENT' | head -1 | grep -q .; then
  scan_c_unsafe 'SQLITE_TRANSIENT' | head -20
  fail "SQLITE_TRANSIENT (use SQLITE_STATIC per project rules)"
else
  pass "no SQLITE_TRANSIENT"
fi

echo ""
echo "Notice: intentional system()/popen() (review when changing):"
if command -v rg &>/dev/null; then
  rg -n --glob '*.c' '\b(system|popen)\s*\(' src/ 2>/dev/null | head -20 || true
else
  find src -name '*.c' -exec grep -n -E '\b(system|popen)\s*\(' {} + 2>/dev/null | head -20 || true
fi

banner "audit-fleet: summary"
if [ "$EXIT_CODE" -eq 0 ]; then
  echo "audit-fleet: all automated checks passed"
  echo "Next: ./build/human_tests (dev preset = ASan) for allocation leaks"
else
  echo "audit-fleet: completed with failures (exit $EXIT_CODE)"
fi

exit "$EXIT_CODE"
