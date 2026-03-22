#!/usr/bin/env bash
# Read-only snapshots from ~/.human/cognition.db (metacog_history).
# Usage: ./scripts/metacog-analytics.sh [path/to/cognition.db]
set -euo pipefail
DB="${1:-${HOME}/.human/cognition.db}"
if [[ ! -f "$DB" ]]; then
    echo "metacog-analytics: no file at $DB"
    echo "  (Create one by running human with HU_ENABLE_SQLITE + metacognition enabled.)"
    exit 0
fi
if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "metacog-analytics: sqlite3 not found in PATH"
    exit 1
fi

echo "=== metacog_history @ $DB ==="
sqlite3 "$DB" <<'SQL'
.headers on
.mode column
SELECT COUNT(*) AS total_rows FROM metacog_history;
SELECT id, trace_id, risk_score, logprob_mean, regen_applied, action, difficulty, timestamp
FROM metacog_history
ORDER BY id DESC
LIMIT 8;
SQL
