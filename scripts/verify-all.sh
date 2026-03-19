#!/usr/bin/env bash
# Combined verification script: build, test, doc index, standards drift, token lint.
# Run before claiming any work done, and as part of the weekly drift audit.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

PASS=0
FAIL=0
SKIP=0

run_check() {
  local name="$1"
  shift
  echo ""
  echo "=== $name ==="
  if "$@"; then
    echo "--- PASS: $name"
    PASS=$((PASS + 1))
  else
    echo "--- FAIL: $name"
    FAIL=$((FAIL + 1))
  fi
}

skip_check() {
  local name="$1"
  local reason="$2"
  echo ""
  echo "=== $name ==="
  echo "--- SKIP: $reason"
  SKIP=$((SKIP + 1))
}

echo "=============================="
echo " human verify-all"
echo "=============================="

# 1. C build
if [ -d "build" ]; then
  run_check "C Build" cmake --build build -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
else
  skip_check "C Build" "build/ directory not found (run cmake -B build first)"
fi

# 2. C tests
if [ -f "build/human_tests" ]; then
  run_check "C Tests" ./build/human_tests
else
  skip_check "C Tests" "build/human_tests not found"
fi

# 3. UI typecheck + lint + test
if [ -f "ui/package.json" ]; then
  if command -v npm &>/dev/null && [ -d "ui/node_modules" ]; then
    run_check "UI Check" npm run check --prefix ui
  else
    skip_check "UI Check" "npm not available or ui/node_modules missing"
  fi
else
  skip_check "UI Check" "ui/package.json not found"
fi

# 4. Standards doc index
if [ -f "scripts/check-doc-index.sh" ]; then
  run_check "Doc Index" bash scripts/check-doc-index.sh
fi

# 5. Standards drift
if [ -f "scripts/check-standards-drift.sh" ]; then
  run_check "Standards Drift" bash scripts/check-standards-drift.sh
fi

# 6. Terminology compliance
if [ -f "scripts/check-terminology.sh" ]; then
  run_check "Terminology" bash scripts/check-terminology.sh
fi

# 7. Token lint (raw colors)
if [ -f "scripts/lint-raw-colors.sh" ]; then
  run_check "Token Lint (colors)" bash scripts/lint-raw-colors.sh --all
fi

# 7. UI token lint
if [ -f "ui/package.json" ] && command -v npm &>/dev/null && [ -d "ui/node_modules" ]; then
  run_check "Token Lint (UI)" npm run lint:tokens --prefix ui 2>/dev/null || true
fi

# 8. Doc stats (display for manual review)
if [ -f "scripts/doc-stats.sh" ]; then
  run_check "Doc Stats" bash scripts/doc-stats.sh
fi

# Summary
echo ""
echo "=============================="
echo " Results: $PASS passed, $FAIL failed, $SKIP skipped"
echo "=============================="

if [ "$FAIL" -gt 0 ]; then
  echo ""
  echo "VERIFICATION FAILED. Fix failures before claiming work is done."
  exit 1
else
  echo ""
  echo "All checks passed."
  exit 0
fi
