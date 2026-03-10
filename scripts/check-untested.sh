#!/usr/bin/env bash
# check-untested.sh — Find src/*.c files with no corresponding test coverage.
# Exit 0 if all files have coverage, exit 1 if gaps found.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

SKIP_PATTERNS="^factory$|^meta_common$|^main$|^main_wasi$|_common$|^bootstrap$|^config_schema$|^cp_admin$|^cp_chat$|^cp_config$|^cp_memory$|^cp_voice$|^thread_pool$|^agent_stream$|^superhuman_predictive$|^embedder_gemini_adapter$|^provider_http$|^data_|^embedded_registry$|^anticipatory$|^conversation_plan$|^info_asymmetry$|^theory_of_mind$|^voice_maturity$"
FOUND=0

while IFS= read -r src; do
    base=$(basename "$src" .c)

    if echo "$base" | grep -qE "$SKIP_PATTERNS"; then
        continue
    fi

    if grep -rql "${base}" tests/ >/dev/null 2>&1; then
        continue
    fi

    echo "  NO TEST: $src"
    FOUND=$((FOUND + 1))
done < <(find src -name '*.c' | sort)

if [ "$FOUND" -eq 0 ]; then
    echo "All source files have test references."
else
    echo ""
    echo "$FOUND source file(s) with no test references found."
fi

if [ "$FOUND" -gt 0 ]; then
    exit 1
fi
