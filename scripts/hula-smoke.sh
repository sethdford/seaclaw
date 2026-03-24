#!/usr/bin/env bash
# Quick HuLa CLI sanity check (parse → validate → run). Requires a built ./build/human binary.
# Optional: HU_HULA_TRACE_DIR persists trace JSON (POSIX); smoke uses a temp dir when unset.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT}/build/human"
PROG='{"name":"smoke","version":1,"root":{"op":"call","id":"c","tool":"echo","args":{"text":"ok"}}}'
TRACE_DIR="${HU_HULA_TRACE_DIR:-}"
if [[ -z "$TRACE_DIR" ]]; then
  TRACE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/hula_smoke_trace_XXXXXX")"
  export HU_HULA_TRACE_DIR="$TRACE_DIR"
  _HULA_SMOKE_RM_TRACE=1
else
  _HULA_SMOKE_RM_TRACE=0
fi

if [[ ! -x "$BIN" ]]; then
    echo "hula-smoke: ${BIN} not found or not executable; run: cmake --build build" >&2
    exit 1
fi

"$BIN" hula validate "$PROG"
"$BIN" hula run "$PROG"
# Prefer newest trace when multiple JSON files exist (e.g. after a prior replay persist).
json_file=""
if compgen -G "$TRACE_DIR"/*.json > /dev/null; then
  # shellcheck disable=SC2012
  json_file="$(ls -t "$TRACE_DIR"/*.json 2>/dev/null | head -1)"
fi
if [[ -n "$json_file" ]]; then
  "$BIN" hula replay "$json_file"
fi
if [[ "$_HULA_SMOKE_RM_TRACE" -eq 1 ]]; then
  n="$(find "$TRACE_DIR" -maxdepth 1 -name '*.json' 2>/dev/null | wc -l | tr -d ' ')"
  rm -rf "$TRACE_DIR"
  echo "hula-smoke: OK (trace persist exercised in temp dir, ${n:-0} file(s))"
else
  echo "hula-smoke: OK (trace dir: $TRACE_DIR)"
fi
