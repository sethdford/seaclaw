#!/usr/bin/env bash
# update-stats.sh — Sync AGENTS.md with actual repo metrics.
# Run from repo root: ./scripts/update-stats.sh
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

# Count source + header files
SRC_COUNT=$(find src include -name '*.c' -o -name '*.h' | wc -l | tr -d ' ')

# Count lines of C
C_LINES=$(find src include -name '*.c' -o -name '*.h' -exec cat {} + | wc -l | tr -d ' ')

# Count test files
TEST_FILES=$(find tests -name 'test_*.c' | wc -l | tr -d ' ')

# Count test lines
TEST_LINES=$(find tests -name '*.c' -o -name '*.h' -exec cat {} + | wc -l | tr -d ' ')

# Get test count from binary if available
if [ -f build/seaclaw_tests ]; then
    TEST_COUNT=$(build/seaclaw_tests 2>/dev/null | grep 'Results:' | sed 's|.*: \([0-9]*\)/.*|\1|' || echo "unknown")
else
    TEST_COUNT="unknown"
fi

# Get binary size
if [ -f build/seaclaw ]; then
    BIN_SIZE=$(stat -f "%z" build/seaclaw 2>/dev/null || stat -c "%s" build/seaclaw 2>/dev/null || echo "unknown")
    BIN_KB=$((BIN_SIZE / 1024))
else
    BIN_KB="unknown"
fi

echo "=== SeaClaw Stats ==="
echo "Source + header files: ${SRC_COUNT}"
echo "Lines of C:           ${C_LINES}"
echo "Test files:           ${TEST_FILES}"
echo "Test lines:           ${TEST_LINES}"
echo "Tests:                ${TEST_COUNT}"
echo "Binary size (debug):  ${BIN_KB} KB"
echo ""
echo "To update AGENTS.md, edit the Project Snapshot section with these values."
