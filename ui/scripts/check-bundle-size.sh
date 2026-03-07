#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# Build for production
npm run build --silent

# Check total JS+CSS asset size
TOTAL=0
for f in dist/assets/*.js dist/assets/*.css; do
  [ -f "$f" ] || continue
  TOTAL=$((TOTAL + $(wc -c < "$f")))
done

KB=$((TOTAL / 1024))
BUDGET=350
PCT=$((KB * 100 / BUDGET))

echo "Bundle size: ${KB} KB / ${BUDGET} KB (${PCT}%)"
if [ "$KB" -gt "$BUDGET" ]; then
  echo "OVER BUDGET by $((KB - BUDGET)) KB"
  exit 1
fi
echo "Within budget"
