#!/usr/bin/env bash
# Red-team + evaluation fleet: offline C tests, eval suite inventory, optional live API runs.
#
# Default (no API spend): C tests (Eval* + Adversarial*), human doctor, human eval list,
#   adversarial-eval-harness dry-run with adversarial + capability_edges suites.
#
# Live (network + provider keys): set REDTEAM_FLEET_LIVE=1
#   - human eval run on a configurable list of JSON suites (reports under REDTEAM_REPORT_DIR)
#   - adversarial-eval-harness.py with synthetic probes + suite (needs ADV_EVAL_API_KEY)
#
# Optional: REDTEAM_FLEET_AGENT_SMOKE=1 runs one human agent -m (uses HUMAN_PROVIDER / keys in env).
# Live harness: REDTEAM_PROBE_PROFILE=safety|capability_honesty for LLM-generated probes (suite tasks use JSON judge_profile).
#
# Examples:
#   bash scripts/redteam-eval-fleet.sh
#   REDTEAM_FLEET_LIVE=1 set -a && source .env && set +a && bash scripts/redteam-eval-fleet.sh
#   REDTEAM_FLEET_LIVE=1 REDTEAM_PROBES=5 REDTEAM_EVAL_SUITES="eval_suites/adversarial.json" bash scripts/redteam-eval-fleet.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

EXIT_CODE=0

REPORT_DIR="${REDTEAM_REPORT_DIR:-$ROOT/build/redteam-fleet-reports}"
HUMAN_BIN="${HUMAN_BIN:-$ROOT/build/human}"
HARNESS="$ROOT/scripts/adversarial-eval-harness.py"
# Space-separated paths, relative to ROOT
DEFAULT_EVAL_SUITES="eval_suites/reasoning_basic.json eval_suites/tool_use_basic.json eval_suites/adversarial.json eval_suites/capability_edges.json eval_suites/coding_basic.json"

run() {
  local name="$1"
  shift
  echo ""
  echo "=== redteam-eval-fleet: $name ==="
  if "$@"; then
    echo "--- OK: $name"
  else
    echo "--- FAIL: $name"
    EXIT_CODE=1
  fi
}

skip() {
  local name="$1"
  local why="$2"
  echo ""
  echo "=== redteam-eval-fleet: $name ==="
  echo "--- SKIP: $why"
}

echo "=============================="
echo " redteam-eval-fleet"
echo "  LIVE=${REDTEAM_FLEET_LIVE:-0} AGENT_SMOKE=${REDTEAM_FLEET_AGENT_SMOKE:-0}"
echo "=============================="

if [ ! -x "$HUMAN_BIN" ]; then
  echo "redteam-eval-fleet: no executable at HUMAN_BIN=$HUMAN_BIN (build the project first)" >&2
  exit 1
fi

if [ ! -f "$HARNESS" ]; then
  echo "redteam-eval-fleet: missing $HARNESS" >&2
  exit 1
fi

# --- Offline: C tests (substring suite filters — avoid --suite=Eval; matches "retrieval") ---
if [ -f "$ROOT/build/human_tests" ]; then
  run "C tests (suite Adversarial)" "$ROOT/build/human_tests" --suite=Adversarial
  run "C tests (suite: Evaluation Harness)" "$ROOT/build/human_tests" --suite="Evaluation Harness"
  run "C tests (suite: Eval Judge)" "$ROOT/build/human_tests" --suite="Eval Judge"
  run "C tests (suite: Eval History Storage)" "$ROOT/build/human_tests" --suite="Eval History"
  run "C tests (suite: eval_runner)" "$ROOT/build/human_tests" --suite=eval_runner
  run "C tests (suite: eval_bench)" "$ROOT/build/human_tests" --suite=eval_bench
  run "C tests (suite: Weakness Analyzer)" "$ROOT/build/human_tests" --suite="Weakness Analyzer"
else
  skip "C tests" "build/human_tests not found"
  EXIT_CODE=1
fi

run "human doctor" "$HUMAN_BIN" doctor

run "human eval list" "$HUMAN_BIN" eval list

# --- Harness dry-run (no API keys): safety + epistemic / anti–AGI-overclaim probes ---
run "adversarial harness (dry-run, adversarial + capability_edges)" \
  python3 "$HARNESS" --dry-run --no-llm \
  --include-suite "$ROOT/eval_suites/adversarial.json" \
  --include-suite "$ROOT/eval_suites/capability_edges.json"

# --- Live: human eval run ---
if [ "${REDTEAM_FLEET_LIVE:-0}" = 1 ]; then
  mkdir -p "$REPORT_DIR"
  TS="$(date -u +%Y%m%dT%H%M%SZ)"
  LIVE_DIR="$REPORT_DIR/live-$TS"
  mkdir -p "$LIVE_DIR"
  echo ""
  echo "=== redteam-eval-fleet: live eval runs -> $LIVE_DIR ==="

  SUITES="${REDTEAM_EVAL_SUITES:-$DEFAULT_EVAL_SUITES}"
  local_fail=0
  for suite in $SUITES; do
    path="$ROOT/${suite#./}"
    base="$(basename "$path" .json)"
    out="$LIVE_DIR/eval-${base}.log"
    echo "  running: $suite"
    if "$HUMAN_BIN" eval run "$path" >"$out" 2>&1; then
      echo "  --- OK: $suite"
    else
      echo "  --- FAIL: $suite (see $out)"
      local_fail=1
    fi
  done
  if [ "$local_fail" -ne 0 ]; then
    EXIT_CODE=1
  fi

  if [ -n "${ADV_EVAL_API_KEY:-}" ]; then
    probes="${REDTEAM_PROBES:-4}"
    probe_prof="${REDTEAM_PROBE_PROFILE:-safety}"
    echo ""
    echo "=== redteam-eval-fleet: dynamic harness (probes=$probes profile=$probe_prof) -> $LIVE_DIR/harness-report.json ==="
    if python3 "$HARNESS" --probes "$probes" --probe-profile "$probe_prof" \
      --include-suite "$ROOT/eval_suites/adversarial.json" \
      --include-suite "$ROOT/eval_suites/capability_edges.json" \
      --timeout "${REDTEAM_AGENT_TIMEOUT:-180}" \
      --output "$LIVE_DIR/harness-report.json" 2>&1 | tee "$LIVE_DIR/harness-console.log"; then
      echo "--- OK: dynamic harness"
    else
      echo "--- FAIL: dynamic harness"
      EXIT_CODE=1
    fi
  else
    skip "dynamic harness (live)" "ADV_EVAL_API_KEY unset"
  fi
else
  skip "human eval run (live)" "REDTEAM_FLEET_LIVE not set to 1"
  skip "dynamic harness (live)" "REDTEAM_FLEET_LIVE not set to 1"
fi

# --- Optional agent smoke ---
if [ "${REDTEAM_FLEET_AGENT_SMOKE:-0}" = 1 ]; then
  msg="${REDTEAM_AGENT_MESSAGE:-Reply with exactly: pong}"
  run "human agent smoke (-m)" "$HUMAN_BIN" agent -m "$msg"
else
  skip "human agent smoke" "REDTEAM_FLEET_AGENT_SMOKE not set to 1"
fi

echo ""
echo "=============================="
if [ "$EXIT_CODE" -eq 0 ]; then
  echo " redteam-eval-fleet: completed (exit 0)"
else
  echo " redteam-eval-fleet: completed with failures (exit $EXIT_CODE)"
fi
echo "=============================="

exit "$EXIT_CODE"
