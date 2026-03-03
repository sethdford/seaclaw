#!/bin/sh
# SeaClaw local benchmarking harness
# Usage: ./scripts/benchmark.sh [path/to/seaclaw] [--compare FILE]
# Measures: binary size, symbols, sections, startup time, memory, test metrics.

set -e

# Default paths (release seaclaw, tests often in build-check with ASan)
SCRIPTS_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPTS_DIR/.." && pwd)
DEFAULT_SEACLAW="${ROOT_DIR}/build/seaclaw"
DEFAULT_TESTS="${ROOT_DIR}/build-check/seaclaw_tests"

# If build-check doesn't exist, try build/ for tests (common single-dir setup)
if [ ! -x "$DEFAULT_TESTS" ] && [ -x "${ROOT_DIR}/build/seaclaw_tests" ]; then
    DEFAULT_TESTS="${ROOT_DIR}/build/seaclaw_tests"
fi

COMPARE_FILE=""
SEACLAW_BIN=""
TESTS_BIN=""

# Parse args
for arg in "$@"; do
    case "$arg" in
        --compare)
            COMPARE_FILE="$2"
            shift 2
            break
            ;;
        --compare=*)
            COMPARE_FILE="${arg#--compare=}"
            shift
            break
            ;;
        *)
            if [ -z "$SEACLAW_BIN" ]; then
                SEACLAW_BIN="$arg"
            fi
            shift
            ;;
    esac
done

# Handle --compare appearing later (simplified parsing)
for arg in "$@"; do
    case "$arg" in
        --compare)
            [ -n "$2" ] && COMPARE_FILE="$2"
            break
            ;;
        --compare=*)
            COMPARE_FILE="${arg#--compare=}"
            break
            ;;
    esac
done

# Resolve paths
if [ -z "$SEACLAW_BIN" ]; then
    SEACLAW_BIN="$DEFAULT_SEACLAW"
fi

# Resolve relative paths
if [ -z "${SEACLAW_BIN##*/*}" ]; then
    case "$SEACLAW_BIN" in
        /*) ;;
        *) SEACLAW_BIN="${ROOT_DIR}/${SEACLAW_BIN}"
    esac
fi

# Tests: same dir as seaclaw first, else default
BIN_DIR=$(dirname "$SEACLAW_BIN")
if [ -x "${BIN_DIR}/seaclaw_tests" ]; then
    TESTS_BIN="${BIN_DIR}/seaclaw_tests"
else
    TESTS_BIN="$DEFAULT_TESTS"
fi

if [ ! -f "$SEACLAW_BIN" ]; then
    echo "Error: seaclaw binary not found at $SEACLAW_BIN"
    echo "Build with: mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel && cmake --build ."
    exit 1
fi

# Detect OS for time/memory
case "$(uname -s)" in
    Darwin)
        TIME_CMD="/usr/bin/time"
        TIME_ARGS="-l"
        TIME_RSS_UNIT="bytes"
        ;;
    Linux|*)
        TIME_CMD="/usr/bin/time"
        TIME_ARGS="-v"
        TIME_RSS_UNIT="kbytes"
        ;;
esac

# Nanosecond timestamp: gdate (macOS), date (Linux), perl fallback
get_nanos() {
    if date +%s%N 2>/dev/null | grep -q '^[0-9][0-9]*$'; then
        date +%s%N
    elif command -v gdate >/dev/null 2>&1; then
        gdate +%s%N
    else
        perl -MTime::HiRes -e 'print int(Time::HiRes::time() * 1e9)' 2>/dev/null || date +%s
    fi
}

# Parse RSS from time output
# macOS: "  1234567  maximum resident set size" (bytes)
# Linux: "Maximum resident set size (kbytes): 1234"
parse_rss() {
    local out="$1"
    local rss=0
    if [ "$TIME_RSS_UNIT" = "bytes" ]; then
        rss=$(echo "$out" | awk '/maximum resident set size/ {gsub(/,/,""); print $1; exit}')
    else
        rss=$(echo "$out" | awk -F: '/Maximum resident set size \(kbytes\)/ {gsub(/[^0-9]/,""); print $2; exit}')
        [ -n "$rss" ] && rss=$((rss * 1024))
    fi
    echo "${rss:-0}"
}

# Human-readable bytes
fmt_bytes() {
    local n="$1"
    if [ "$n" -ge 1048576 ]; then
        echo "${n} bytes ($((n / 1048576)) MB)"
    elif [ "$n" -ge 1024 ]; then
        echo "${n} bytes ($((n / 1024)) KB)"
    else
        echo "${n} bytes"
    fi
}

# Colored output (optional)
red=''
green=''
reset=''
if [ -t 1 ]; then
    red='\033[0;31m'
    green='\033[0;32m'
    reset='\033[0m'
fi

# --- Collect metrics ---

# Binary size
BINARY_SIZE=$(wc -c < "$SEACLAW_BIN" | tr -d ' ')
BINARY_SIZE_HR=$(printf "%'d" "$BINARY_SIZE")
if [ "$BINARY_SIZE" -ge 1024 ]; then
    BINARY_SIZE_KB=$((BINARY_SIZE / 1024))
    BINARY_HR="${BINARY_SIZE_KB} KB (${BINARY_SIZE_HR} bytes)"
else
    BINARY_HR="${BINARY_SIZE_HR} bytes"
fi

# Exported symbols
SYMBOL_COUNT=0
if command -v nm >/dev/null 2>&1; then
    SYMBOL_COUNT=$(nm -g "$SEACLAW_BIN" 2>/dev/null | wc -l | tr -d ' ')
fi
SYMBOL_HR=$(printf "%'d" "$SYMBOL_COUNT")

# Section sizes (text, data, bss)
TEXT_SIZE=0
DATA_SIZE=0
BSS_SIZE=0
if command -v size >/dev/null 2>&1; then
    size_out=$(size "$SEACLAW_BIN" 2>/dev/null)
    TEXT_SIZE=$(echo "$size_out" | tail -1 | awk '{print $1}')
    DATA_SIZE=$(echo "$size_out" | tail -1 | awk '{print $2}')
    BSS_SIZE=$(echo "$size_out" | tail -1 | awk '{print $3}')
fi
TEXT_HR=""
[ "$TEXT_SIZE" -gt 0 ] && TEXT_HR="$((TEXT_SIZE / 1024)) KB"

# Startup time (10 runs)
STARTUP_MIN=999999
STARTUP_MAX=0
STARTUP_SUM=0
STARTUP_RUNS=10
i=0
while [ "$i" -lt "$STARTUP_RUNS" ]; do
    t0=$(get_nanos)
    "$SEACLAW_BIN" --version >/dev/null 2>&1
    t1=$(get_nanos)
    # Handle both nanoseconds and seconds (perl fallback)
    if [ "${#t0}" -ge 10 ]; then
        elaps=$((t1 - t0))
    else
        elaps=$(((t1 - t0) * 1000000000))
    fi
    elaps_ms=$((elaps / 1000000))
    [ "$elaps_ms" -lt "$STARTUP_MIN" ] && STARTUP_MIN=$elaps_ms
    [ "$elaps_ms" -gt "$STARTUP_MAX" ] && STARTUP_MAX=$elaps_ms
    STARTUP_SUM=$((STARTUP_SUM + elaps_ms))
    i=$((i + 1))
done
STARTUP_AVG=$((STARTUP_SUM / STARTUP_RUNS))

# Memory: peak RSS during tests
TESTS_RSS=0
TESTS_DURATION=0
if [ -x "$TESTS_BIN" ]; then
    tests_dir=$(dirname "$TESTS_BIN")
    time_out=$("$TIME_CMD" $TIME_ARGS "$TESTS_BIN" 2>&1 || true)
    TESTS_RSS=$(parse_rss "$time_out")
    # Duration from time "real" line
    if echo "$time_out" | grep -q 'real'; then
        real_line=$(echo "$time_out" | grep 'real' | head -1)
        # Format: "0.45 real" or "        0.45 real"
        real_sec=$(echo "$real_line" | awk '{print $1}' | tr -d ' ')
        TESTS_DURATION=$(echo "$real_sec" | awk '{printf "%.2f", $1}')
    fi
else
    TESTS_RSS=0
fi

# Memory: peak RSS during --version
VERSION_RSS=0
time_out=$("$TIME_CMD" $TIME_ARGS "$SEACLAW_BIN" --version 2>&1 || true)
VERSION_RSS=$(parse_rss "$time_out")

# Test count and duration (if we ran tests and got output)
TEST_COUNT=0
if [ -x "$TESTS_BIN" ]; then
    test_out=$("$TESTS_BIN" 2>&1 || true)
    # Parse "Results: 2002/2002 passed" or "Results: 1998/2002 passed, 4 FAILED"
    match=$(echo "$test_out" | grep -o 'Results: [0-9]*/[0-9]* passed' | tail -1)
    if [ -n "$match" ]; then
        TEST_COUNT=$(echo "$match" | sed 's/.*\/\([0-9]*\) passed/\1/')
    fi
fi

# Re-run tests for duration if we need it (we already have it from time)
if [ -x "$TESTS_BIN" ] && [ "$TESTS_DURATION" = "0" ]; then
    t0=$(get_nanos)
    "$TESTS_BIN" >/dev/null 2>&1 || true
    t1=$(get_nanos)
    if [ "${#t0}" -ge 10 ]; then
        elaps=$((t1 - t0))
    else
        elaps=$(((t1 - t0) * 1000000000))
    fi
    TESTS_DURATION=$(echo "scale=2; $elaps / 1000000000" | bc 2>/dev/null || echo "0")
fi

# Tests rate
RATE=0
if [ "$TEST_COUNT" -gt 0 ] && [ "$TESTS_DURATION" != "0" ]; then
    RATE=$(echo "scale=0; $TEST_COUNT / $TESTS_DURATION" | bc 2>/dev/null || echo "0")
fi

TEST_COUNT_HR=$(printf "%'d" "$TEST_COUNT")
TESTS_RSS_HR=$(fmt_bytes "$TESTS_RSS")
VERSION_RSS_HR=$(fmt_bytes "$VERSION_RSS")

# --- Output JSON ---
JSON_FILE="${ROOT_DIR}/benchmark-results.json"
cat > "$JSON_FILE" << EOF
{
  "binary": {
    "path": "$SEACLAW_BIN",
    "size_bytes": $BINARY_SIZE,
    "size_human": "$BINARY_HR",
    "symbols_exported": $SYMBOL_COUNT,
    "section_text_bytes": $TEXT_SIZE
  },
  "startup_ms": {
    "min": $STARTUP_MIN,
    "avg": $STARTUP_AVG,
    "max": $STARTUP_MAX,
    "runs": $STARTUP_RUNS
  },
  "memory": {
    "peak_rss_tests_bytes": $TESTS_RSS,
    "peak_rss_version_bytes": $VERSION_RSS
  },
  "tests": {
    "count": $TEST_COUNT,
    "duration_sec": $TESTS_DURATION,
    "rate_per_sec": $RATE
  }
}
EOF

# --- Print table ---
print_row() {
    printf "  %-20s  %s\n" "$1" "$2"
}

print_section() {
    echo ""
    echo "  $1"
    echo "  $(printf '%*s' 40 '' | tr ' ' '\255' | tr '\255' '\055')"
}

echo ""
echo "═══════════════════════════════════════════════"
echo "  SeaClaw Benchmark Report"
echo "═══════════════════════════════════════════════"

print_section "Binary"
print_row "Size:" "$BINARY_HR"
print_row "Symbols:" "${SYMBOL_HR} exported"
[ -n "$TEXT_HR" ] && print_row "Text section:" "$TEXT_HR"

print_section "Startup (10 runs)"
print_row "Min:" "${STARTUP_MIN} ms"
print_row "Avg:" "${STARTUP_AVG} ms"
print_row "Max:" "${STARTUP_MAX} ms"

print_section "Memory"
print_row "Peak RSS (tests):" "$TESTS_RSS_HR"
print_row "Peak RSS (--version):" "$VERSION_RSS_HR"

print_section "Tests"
print_row "Count:" "$TEST_COUNT_HR"
print_row "Duration:" "${TESTS_DURATION}s"
print_row "Rate:" "$(printf "%'d" "$RATE") tests/sec"

echo ""
echo "═══════════════════════════════════════════════"
echo "Results saved to: $JSON_FILE"
echo ""

# --- Compare with previous ---
if [ -n "$COMPARE_FILE" ] && [ -f "$COMPARE_FILE" ]; then
    echo "Comparing with $COMPARE_FILE:"
    "${SCRIPTS_DIR}/compare-benchmarks.sh" "$COMPARE_FILE" "$JSON_FILE" 2>/dev/null || true
fi
