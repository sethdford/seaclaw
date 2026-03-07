#!/usr/bin/env bash
# Visual regression: capture screenshots of the component catalog and key views
# Usage: bash scripts/visual-regression.sh [--update]
set -euo pipefail

cd "$(dirname "$0")/.."

SCREENSHOT_DIR="visual-regression"
BASELINE_DIR="visual-regression/baseline"
DIFF_DIR="visual-regression/diff"
UPDATE_MODE=false

if [ "${1:-}" = "--update" ]; then
  UPDATE_MODE=true
fi

mkdir -p "$SCREENSHOT_DIR" "$DIFF_DIR"

# Build first
npm run build

# Start preview server
npx vite preview --port 4174 &
SERVER_PID=$!
trap "kill $SERVER_PID 2>/dev/null" EXIT
sleep 3

PAGES=(
  "/:overview"
  "/#chat:chat"
  "/#config:config"
  "/catalog.html:catalog"
)

FAILURES=0

for entry in "${PAGES[@]}"; do
  IFS=: read -r path name <<< "$entry"
  SCREENSHOT="$SCREENSHOT_DIR/${name}.png"
  
  npx playwright screenshot --browser chromium "http://localhost:4174${path}" "$SCREENSHOT"
  
  if [ "$UPDATE_MODE" = true ]; then
    mkdir -p "$BASELINE_DIR"
    cp "$SCREENSHOT" "$BASELINE_DIR/${name}.png"
    echo "Updated baseline: ${name}.png"
  elif [ -f "$BASELINE_DIR/${name}.png" ]; then
    # Compare with baseline (pixel diff using ImageMagick if available)
    if command -v compare &>/dev/null; then
      DIFF_METRIC=$(compare -metric AE "$BASELINE_DIR/${name}.png" "$SCREENSHOT" "$DIFF_DIR/${name}-diff.png" 2>&1 || true)
      if ! [[ "${DIFF_METRIC:-0}" =~ ^[0-9]+$ ]] || [ "${DIFF_METRIC:-0}" -gt 100 ]; then
        echo "VISUAL DIFF: ${name} changed (${DIFF_METRIC} pixels differ)"
        FAILURES=$((FAILURES + 1))
      else
        echo "PASS: ${name} (${DIFF_METRIC} pixels)"
      fi
    else
      echo "SKIP: ImageMagick not installed, cannot compare ${name}"
      FAILURES=$((FAILURES + 1))
    fi
  else
    echo "NO BASELINE: ${name} — run with --update to create"
    FAILURES=$((FAILURES + 1))
  fi
done

if [ "$FAILURES" -gt 0 ]; then
  echo ""
  echo "${FAILURES} visual regression(s) detected. Review diff images in ${DIFF_DIR}/"
  exit 1
fi

echo "Visual regression check complete."
