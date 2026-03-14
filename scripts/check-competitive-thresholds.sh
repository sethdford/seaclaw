#!/usr/bin/env bash
# check-competitive-thresholds.sh — Enforce h-uman competitive performance thresholds
# Reads benchmark-competitive.json and fails if h-uman scores drop below targets.
# Used by competitive-benchmark.yml CI workflow.
set -euo pipefail

RESULTS_FILE="${1:-benchmark-competitive.json}"

if [ ! -f "$RESULTS_FILE" ]; then
  echo "::warning::No benchmark results found at $RESULTS_FILE, skipping threshold check"
  exit 0
fi

PERF_THRESHOLD=95
A11Y_THRESHOLD=98

PERF=$(jq -r '.[] | select(.name == "Human") | .performance // 0' "$RESULTS_FILE")
A11Y=$(jq -r '.[] | select(.name == "Human") | .accessibility // 0' "$RESULTS_FILE")

echo "h-uman Performance: $PERF (threshold: $PERF_THRESHOLD)"
echo "h-uman Accessibility: $A11Y (threshold: $A11Y_THRESHOLD)"

FAIL=0

if [ "$(echo "$PERF < $PERF_THRESHOLD" | bc -l 2>/dev/null || echo 0)" -eq 1 ]; then
  echo "::error::Performance score $PERF is below the $PERF_THRESHOLD threshold"
  FAIL=1
fi

if [ "$(echo "$A11Y < $A11Y_THRESHOLD" | bc -l 2>/dev/null || echo 0)" -eq 1 ]; then
  echo "::error::Accessibility score $A11Y is below the $A11Y_THRESHOLD threshold"
  FAIL=1
fi

if [ "$FAIL" -eq 1 ]; then
  exit 1
fi

echo "::notice::h-uman passed competitive benchmark thresholds"
