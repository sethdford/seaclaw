#!/usr/bin/env bash
# Print canonical repository metrics for documentation accuracy.
# Run this script to get ground-truth values before updating .md files.
# Wired into verify-all.sh for weekly drift audits.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

src_c=$(find src -name "*.c" | wc -l | tr -d ' ')
inc_h=$(find include -name "*.h" | wc -l | tr -d ' ')
total_source=$((src_c + inc_h))
test_files=$(find tests -name "test_*.c" | wc -l | tr -d ' ')
channels=$(ls src/channels/*.c 2>/dev/null | wc -l | tr -d ' ')
tools=$(find src/tools -name "*.c" | wc -l | tr -d ' ')
providers=$(find src/providers -name "*.c" | wc -l | tr -d ' ')
memory_engines=$(find src/memory/engines -name "*.c" 2>/dev/null | wc -l | tr -d ' ')
fuzz_harnesses=$(find fuzz -name "fuzz_*.c" 2>/dev/null | wc -l | tr -d ' ')
lines_src=$(find src -name "*.c" -exec cat {} + | wc -l | tr -d ' ')
lines_tests=$(find tests -name "*.c" -exec cat {} + | wc -l | tr -d ' ')

ci_baseline=""
if [ -f ".github/workflows/ci.yml" ]; then
  ci_baseline=$(grep -o 'BASELINE=[0-9]*' .github/workflows/ci.yml 2>/dev/null | head -1 | cut -d= -f2 || echo "")
fi

echo "=============================="
echo " h-uman doc stats"
echo "=============================="
echo ""
echo "Source files (.c in src/):     $src_c"
echo "Header files (.h in include/): $inc_h"
echo "Total source + header:         $total_source"
echo "Lines of C (src/):             ~$((lines_src / 1000))K"
echo "Test files:                    $test_files"
echo "Lines of tests:                ~$((lines_tests / 1000))K"
echo "CI test baseline:              ${ci_baseline:-unknown}"
echo "Channels:                      $channels"
echo "Tools:                         $tools"
echo "Providers (.c files):          $providers"
echo "Memory engines:                $memory_engines"
echo "Fuzz harnesses:                $fuzz_harnesses"
