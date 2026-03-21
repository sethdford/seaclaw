#!/usr/bin/env bash
# One-shot live red-team fleet: source repo .env (if present), align eval + harness to OpenAI when possible, run fleet.
#
# Requires: build/human, OPENAI_API_KEY in .env (or environment) for full live stack.
#
# Usage:
#   bash scripts/redteam-live.sh
#   REDTEAM_EVAL_SUITES="eval_suites/adversarial.json" bash scripts/redteam-live.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [ -f "$ROOT/.env" ]; then
  set -a
  # shellcheck source=/dev/null
  source "$ROOT/.env"
  set +a
fi

export REDTEAM_FLEET_LIVE=1
export REDTEAM_HARNESS_USE_OPENAI=1
export REDTEAM_AGENT_USE_OPENAI=1

exec bash "$ROOT/scripts/redteam-eval-fleet.sh"
