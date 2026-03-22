#!/usr/bin/env bash
# Merge docs/examples/config.agent.metacognition.json into ~/.human/config.json using jq.
# Backs up the target to config.json.bak before writing.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FRAG="$ROOT/docs/examples/config.agent.metacognition.json"
TARGET="${HOME}/.human/config.json"
if [[ ! -f "$FRAG" ]]; then
    echo "missing $FRAG"
    exit 1
fi
if ! command -v jq >/dev/null 2>&1; then
    echo "jq is required (brew install jq)"
    exit 1
fi
mkdir -p "$(dirname "$TARGET")"
if [[ -f "$TARGET" ]]; then
    cp -a "$TARGET" "${TARGET}.bak"
    jq -s '.[0] * .[1]' "$TARGET" "$FRAG" >"${TARGET}.tmp"
    mv "${TARGET}.tmp" "$TARGET"
    echo "Merged metacognition into $TARGET (backup: ${TARGET}.bak)"
else
    cp "$FRAG" "$TARGET"
    echo "Created $TARGET from example fragment"
fi
