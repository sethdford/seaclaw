#!/bin/bash
# enable-pwa.sh — Enable Chrome JS from Apple Events and verify PWA system
set -e

HUMAN="$(cd "$(dirname "$0")/.." && pwd)/build/human"

echo "================================================"
echo "  Human PWA System — Setup & Verification"
echo "================================================"
echo ""

# Step 1: Check Chrome
if ! pgrep -q "Google Chrome"; then
    echo "[!] Chrome is not running. Opening Chrome..."
    open -a "Google Chrome"
    sleep 3
fi

# Step 2: Guide the user to enable JS
echo "[1] Enable JavaScript from Apple Events in Chrome:"
echo ""
echo "    In Chrome's menu bar, click:"
echo "      View  >  Developer  >  Allow JavaScript from Apple Events"
echo ""
echo "    (It should show a checkmark when enabled)"
echo ""
read -p "    Press ENTER when done... "

# Step 3: Test
echo ""
echo "[2] Testing JS execution..."
RESULT=$(osascript -e 'tell application "Google Chrome" to execute front window'\''s active tab javascript "1+1"' 2>&1)
if echo "$RESULT" | grep -q "turned off"; then
    echo "    [FAIL] JS from Apple Events is still disabled."
    echo "    Please try again: View > Developer > Allow JavaScript from Apple Events"
    exit 1
fi
echo "    [OK] JS execution works! (1+1 = $RESULT)"

# Step 4: Run PWA scan
echo ""
echo "[3] Scanning all open PWA tabs..."
echo ""
"$HUMAN" pwa scan

# Step 5: Run PWA context
echo ""
echo "[4] Building cross-app context..."
echo ""
"$HUMAN" pwa context

# Step 6: Run learn
echo ""
echo "[5] Ingesting PWA content into memory..."
echo ""
"$HUMAN" pwa learn

# Step 7: Run digest
echo ""
echo "[6] Generating digest..."
echo ""
"$HUMAN" pwa digest

echo ""
echo "================================================"
echo "  PWA System is LIVE!"
echo ""
echo "  Cron jobs registered:"
"$HUMAN" cron list 2>/dev/null || echo "  (cron disabled in this build)"
echo ""
echo "  Commands:"
echo "    human pwa scan     — one-shot scan"
echo "    human pwa watch    — continuous monitoring"
echo "    human pwa learn    — ingest into memory"
echo "    human pwa digest   — combined summary"
echo "    human pwa context  — cross-app context"
echo "================================================"
