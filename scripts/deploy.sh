#!/bin/bash
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
PLIST_LABEL="ai.human.service-loop"
PLIST_PATH="$HOME/Library/LaunchAgents/${PLIST_LABEL}.plist"
INSTALL_PATH="$HOME/bin/human"
BUILD_DIR="$REPO/build-release"
LOG_DIR="$HOME/.human/logs"

mkdir -p "$LOG_DIR"

echo "[deploy] Building release..."
cmake --preset release -S "$REPO" >/dev/null 2>&1
cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu)" 2>&1 | tail -3

echo "[deploy] Running tests..."
"$BUILD_DIR/human_tests" 2>&1 | tail -3
TEST_EXIT=$?
if [ $TEST_EXIT -ne 0 ]; then
    echo "[deploy] ABORT: tests failed"
    exit 1
fi

echo "[deploy] Stopping service..."
launchctl bootout "gui/$(id -u)/$PLIST_LABEL" 2>/dev/null || true
"$BUILD_DIR/human" service stop 2>/dev/null || true
pkill -f "bin/human service-loop" 2>/dev/null || true
rm -f "$HOME/.human/human.pid"
sleep 2

echo "[deploy] Installing binary..."
cp "$BUILD_DIR/human" "$INSTALL_PATH"
chmod +x "$INSTALL_PATH"
codesign --force --sign - "$INSTALL_PATH" 2>/dev/null
xattr -cr "$INSTALL_PATH" 2>/dev/null
ls -lh "$INSTALL_PATH"

echo "[deploy] Smoke test..."
"$INSTALL_PATH" doctor 2>&1 | head -5

echo "[deploy] Starting service via launchd..."
launchctl bootstrap "gui/$(id -u)" "$PLIST_PATH" 2>/dev/null || \
    launchctl kickstart -k "gui/$(id -u)/$PLIST_LABEL" 2>/dev/null || true
sleep 3

if launchctl print "gui/$(id -u)/$PLIST_LABEL" 2>/dev/null | grep -q "state = running"; then
    echo "[deploy] Service running (launchd managed, auto-restart on crash)"
else
    echo "[deploy] Warning: launchd status unclear, checking process..."
    pgrep -lf "bin/human service" || echo "[deploy] No service process found"
fi

echo "[deploy] Checking logs..."
tail -5 "$LOG_DIR/service-loop.log" 2>/dev/null || echo "(no logs yet)"
echo ""
echo "[deploy] Done. Binary: $INSTALL_PATH ($(wc -c < "$INSTALL_PATH" | tr -d ' ') bytes)"
