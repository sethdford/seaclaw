#!/usr/bin/env bash
# Quick HuLa CLI sanity check (parse → validate → run). Requires a built ./build/human binary.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT}/build/human"
PROG='{"name":"smoke","version":1,"root":{"op":"call","id":"c","tool":"echo","args":{"text":"ok"}}}'

if [[ ! -x "$BIN" ]]; then
    echo "hula-smoke: ${BIN} not found or not executable; run: cmake --build build" >&2
    exit 1
fi

"$BIN" hula validate "$PROG"
"$BIN" hula run "$PROG"
echo "hula-smoke: OK"
