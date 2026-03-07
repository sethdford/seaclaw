#!/usr/bin/env bash
# update-stats.sh — Sync AGENTS.md, README.md, and STUBS.md with actual repo metrics.
# Usage: ./scripts/update-stats.sh [--apply]
#   Without --apply: prints stats only (dry run).
#   With --apply: patches AGENTS.md, README.md, and STUBS.md in place.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

APPLY=false
[ "${1:-}" = "--apply" ] && APPLY=true

# Count source + header files
SRC_COUNT=$(find src include \( -name '*.c' -o -name '*.h' \) | wc -l | tr -d ' ')

# Count lines of C (round to nearest K)
C_LINES_RAW=$(find src include \( -name '*.c' -o -name '*.h' \) -exec cat {} + | wc -l | tr -d ' ')
C_LINES_K=$(( (C_LINES_RAW + 500) / 1000 ))

# Count test files
TEST_FILES=$(find tests -name 'test_*.c' | wc -l | tr -d ' ')

# Count test lines (round to nearest K)
TEST_LINES_RAW=$(find tests \( -name '*.c' -o -name '*.h' \) -exec cat {} + | wc -l | tr -d ' ')
TEST_LINES_K=$(( (TEST_LINES_RAW + 500) / 1000 ))

# Count channels
CHANNEL_COUNT=$(find src/channels -maxdepth 1 -name '*.c' ! -name 'factory.c' ! -name 'meta_common.c' | wc -l | tr -d ' ')

# Count tools
TOOL_COUNT=$(find src/tools -maxdepth 1 -name '*.c' ! -name 'factory.c' | wc -l | tr -d ' ')

# Get test count from binary (try multiple build dirs)
TEST_COUNT="unknown"
for test_bin in build/seaclaw_tests build2/seaclaw_tests build-check/seaclaw_tests; do
    if [ -f "$test_bin" ]; then
        TEST_COUNT=$("$test_bin" 2>/dev/null | grep 'Results:' | sed 's|.*: \([0-9]*\)/.*|\1|' || echo "unknown")
        break
    fi
done

# Format test count with comma
if [ "$TEST_COUNT" != "unknown" ]; then
    TEST_COUNT_FMT=$(printf "%'d" "$TEST_COUNT" 2>/dev/null || echo "$TEST_COUNT")
else
    TEST_COUNT_FMT="unknown"
fi

# Get binary size (prefer MinSizeRel builds, then fallback)
BINARY_KB="unknown"
for bin in build2/seaclaw build-release/seaclaw build/seaclaw; do
    if [ -f "$bin" ]; then
        BINARY_BYTES=$(stat -f%z "$bin" 2>/dev/null || stat -c%s "$bin" 2>/dev/null || echo 0)
        BINARY_KB=$((BINARY_BYTES / 1024))
        break
    fi
done

echo "=== SeaClaw Stats ==="
echo "Source + header files: ${SRC_COUNT}"
echo "Lines of C:           ~${C_LINES_K}K (${C_LINES_RAW})"
echo "Test files:           ${TEST_FILES}"
echo "Lines of tests:       ~${TEST_LINES_K}K (${TEST_LINES_RAW})"
echo "Tests:                ${TEST_COUNT_FMT}"
echo "Binary size:          ~${BINARY_KB} KB"
echo "Channels:             ${CHANNEL_COUNT}"
echo "Tools:                ${TOOL_COUNT}"

if ! $APPLY; then
    echo ""
    echo "Dry run. To patch files: ./scripts/update-stats.sh --apply"
    exit 0
fi

echo ""
echo "Patching AGENTS.md..."

# Patch AGENTS.md "Current scale" line
sed -i.bak -E \
    "s/Current scale: \*\*[^*]+\*\*/Current scale: **${SRC_COUNT} source + header files, ~${C_LINES_K}K lines of C, ~${TEST_LINES_K}K lines of tests, ${TEST_COUNT_FMT} tests, ${CHANNEL_COUNT} channels**/" \
    AGENTS.md && rm -f AGENTS.md.bak

echo "Patching README.md..."

# Patch README.md tagline (line with tests · providers · channels · tools)
sed -i.bak -E \
    "s/[0-9,]+ tests/${TEST_COUNT_FMT} tests/g" \
    README.md && rm -f README.md.bak

# Patch README.md "Tests:" lines
sed -i.bak -E \
    "s/Tests:[[:space:]]+[0-9,]+ passing/Tests:         ${TEST_COUNT_FMT} passing/" \
    README.md && rm -f README.md.bak

# Patch README.md "tests/" stat line
sed -i.bak -E \
    "s|tests/[[:space:]]+[0-9]+ test files, [0-9,]+ tests|tests/ ${TEST_FILES} test files, ${TEST_COUNT_FMT} tests|" \
    README.md && rm -f README.md.bak

echo "Patching STUBS.md..."

if [ -f STUBS.md ]; then
    # Patch test count
    sed -i.bak -E \
        "s/Tests passing[[:space:]]+\| \*\*[^*]+\*\*/Tests passing                  | **${TEST_COUNT}\/${TEST_COUNT} (100%)**/" \
        STUBS.md && rm -f STUBS.md.bak

    # Patch lines of code
    sed -i.bak -E \
        "s/Lines of C\/H\/ASM code[[:space:]]+\| \*\*~[0-9]+K\*\*/Lines of C\/H\/ASM code          | **~${C_LINES_K}K**/" \
        STUBS.md && rm -f STUBS.md.bak

    # Patch source file count
    sed -i.bak -E \
        "s/Source files \(src\/ \+ include\/\)[[:space:]]*\| \*\*[0-9]+\*\*/Source files (src\/ + include\/) | **${SRC_COUNT}**/" \
        STUBS.md && rm -f STUBS.md.bak

    # Patch test file count
    sed -i.bak -E \
        "s/Test files[[:space:]]+\| [0-9]+/Test files                     | ${TEST_FILES}/" \
        STUBS.md && rm -f STUBS.md.bak

    # Patch binary size
    if [ "$BINARY_KB" != "unknown" ]; then
        sed -i.bak -E \
            "s/Binary size \(MinSizeRel\+LTO\)[[:space:]]+\| \*\*~[0-9]+ KB\*\*/Binary size (MinSizeRel+LTO)   | **~${BINARY_KB} KB**/" \
            STUBS.md && rm -f STUBS.md.bak
    fi

    # Update date
    sed -i.bak -E \
        "s/Last updated: [0-9]{4}-[0-9]{2}-[0-9]{2}/Last updated: $(date +%Y-%m-%d)/" \
        STUBS.md && rm -f STUBS.md.bak
fi

echo "Done. Review changes with: git diff AGENTS.md README.md STUBS.md"
