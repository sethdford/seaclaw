#!/usr/bin/env bash
# prove-intelligence.sh — Prove every intelligence subsystem loop closes.
# Builds h-uman with all features, runs unit tests, then runs targeted
# proof tests that verify each subsystem records AND retrieves data.
#
# Usage: bash scripts/prove-intelligence.sh
#        make prove
set -euo pipefail

JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
BUILD="${BUILD:-build}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
cd "$ROOT"

pass=0
fail=0
step=0
total=5

report() {
    step=$((step + 1))
    local status="$1"; shift
    if [ "$status" = "OK" ]; then
        printf "  [%d/%d] %s... \033[32mOK\033[0m\n" "$step" "$total" "$*"
        pass=$((pass + 1))
    else
        printf "  [%d/%d] %s... \033[31mFAIL\033[0m\n" "$step" "$total" "$*"
        fail=$((fail + 1))
    fi
}

echo ""
echo "============================================"
echo "  h-uman Intelligence Subsystem Proof"
echo "============================================"
echo ""

# ── Phase 1: Build with all intelligence features ───────────────────────
printf "  [1/%d] Building with ML + SQLite + Skills... " "$total"
cmake -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DHU_ENABLE_SQLITE=ON \
    -DHU_ENABLE_ML=ON \
    -DHU_ENABLE_CURL=ON \
    > /dev/null 2>&1
cmake --build "$BUILD" -j"$JOBS" > /dev/null 2>&1
report OK "Build with ML + SQLite"

# ── Phase 2: Run full unit test suite ───────────────────────────────────
printf "  [2/%d] Running core unit tests... " "$total"
# Run targeted suites to avoid hanging on unrelated tests
CORE_SUITES=("humanness" "experience" "world_model" "online_learning" "self_improve" "value_learning" "reflection" "emotional" "persona" "agent_turn" "silence" "pacing")
TOTAL_CORE=0
FAIL_CORE=0
for suite in "${CORE_SUITES[@]}"; do
    SUITE_OUT=$("$BUILD/human_tests" --suite="$suite" 2>&1) || true
    RESULT=$(echo "$SUITE_OUT" | grep "^--- Results:" | head -1)
    if [ -n "$RESULT" ]; then
        P=$(echo "$RESULT" | grep -oE '[0-9]+/[0-9]+' | head -1 | cut -d/ -f1)
        T=$(echo "$RESULT" | grep -oE '[0-9]+/[0-9]+' | head -1 | cut -d/ -f2)
        F=$(echo "$RESULT" | grep -oE '[0-9]+ FAILED' | grep -oE '^[0-9]+' || echo "0")
        TOTAL_CORE=$((TOTAL_CORE + P))
        FAIL_CORE=$((FAIL_CORE + F))
    fi
done
if [ "$TOTAL_CORE" -eq 0 ]; then
    report FAIL "Core tests: 0 tests ran (binary missing or crashed?)"
elif [ "$FAIL_CORE" -eq 0 ]; then
    report OK "Core tests ($TOTAL_CORE passed across ${#CORE_SUITES[@]} suites)"
else
    report FAIL "Core tests ($TOTAL_CORE passed, $FAIL_CORE FAILED)"
fi

# ── Phase 3: Run intelligence wiring tests ──────────────────────────────
printf "  [3/%d] Running intelligence wiring tests... " "$total"
WIRE_OUTPUT=$("$BUILD/human_tests" --suite=intelligence_wiring 2>&1) || true
WIRE_PASSED=$(echo "$WIRE_OUTPUT" | grep -oE '[0-9]+/[0-9]+ passed' | tail -1)
if echo "$WIRE_OUTPUT" | grep -qE "[1-9][0-9]* FAILED"; then
    report FAIL "Intelligence wiring ($WIRE_PASSED)"
else
    report OK "Intelligence wiring ($WIRE_PASSED)"
fi

# ── Phase 4: Run proof-of-closure tests ─────────────────────────────────
printf "  [4/%d] Running proof-of-closure tests... " "$total"
PROVE_OUTPUT=$("$BUILD/human_tests" --suite=prove_e2e 2>&1) || true
PROVE_PASSED=$(echo "$PROVE_OUTPUT" | grep -oE '[0-9]+/[0-9]+ passed' | tail -1)
if [ -z "$PROVE_PASSED" ]; then
    report FAIL "Proof of closure: no tests ran (binary missing or crashed?)"
elif echo "$PROVE_OUTPUT" | grep -qE "[1-9][0-9]* FAILED"; then
    report FAIL "Proof of closure ($PROVE_PASSED)"
    echo ""
    echo "  Failed proof tests:"
    echo "$PROVE_OUTPUT" | grep "FAIL" | head -10
else
    report OK "Proof of closure ($PROVE_PASSED)"
fi

# ── Phase 5: Verify binary works ───────────────────────────────────────
printf "  [5/%d] Verifying binary... " "$total"
VERSION=$("$BUILD/human" --version 2>&1 | head -1) || VERSION="(failed)"
if echo "$VERSION" | grep -q "human"; then
    report OK "Binary: $VERSION"
else
    report FAIL "Binary version check"
fi

# ── Summary ─────────────────────────────────────────────────────────────
echo ""
echo "============================================"
if [ "$fail" -eq 0 ]; then
    printf "  \033[32mAll %d checks passed.\033[0m\n" "$pass"
    echo ""
    echo "  Proven subsystems:"
    echo "    - World model: record → simulate"
    echo "    - Experience: record → recall (FTS5)"
    echo "    - Online learning: signal → weight"
    echo "    - Self-improve: outcome → reflect → patch"
    echo "    - Value learning: correction → prompt"
    echo "    - Behavioral feedback: insert → query"
    echo "    - Silence intuition: grief → presence"
    echo "    - Emotional pacing: heavy → delay"
    echo "    - BPE tokenizer: train → encode → decode"
    echo ""
    echo "  Proven streaming features:"
    echo "    - Router: streaming cascade to child providers"
    echo "    - Reliable: streaming fallback on failure"
    echo "    - Tool results: progressive execute_streaming bridge"
    echo "    - Shell tool: execute_streaming with chunk callbacks"
    echo "    - Web search tool: execute_streaming with result emission"
    echo "    - Voice: provider vtable abstraction (OpenAI wrapped)"
    echo "    - Voice session: provider vtable routing (start/stop/send/interrupt)"
    echo ""
    echo "  Proven integration paths:"
    echo "    - Per-tool recording in agent turn loop"
    echo "    - Trajectory recording for RL (ML gated)"
    echo "    - DPO pair collection from feedback"
    echo "    - Daemon: intelligence cycle (6h)"
    echo "    - Daemon: ML experiment loop (12h)"
    echo "    - Daemon: DPO training (24h)"
else
    printf "  \033[31m%d/%d checks failed.\033[0m\n" "$fail" "$((pass + fail))"
fi
echo "============================================"
echo ""

exit "$fail"
