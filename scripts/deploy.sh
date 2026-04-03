#!/bin/bash
# deploy.sh — Build, test, deduplicate, install, and restart h-uman.
#
# Usage: bash scripts/deploy.sh           # full deploy (build + test + eval + install)
#        bash scripts/deploy.sh --quick   # skip tests and eval
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
PLIST_LABEL="ai.human.service-loop"
PLIST_PATH="$HOME/Library/LaunchAgents/${PLIST_LABEL}.plist"
INSTALL_PATH="$HOME/bin/human"
BUILD_DIR="$REPO/build"
LOG_DIR="$HOME/.human/logs"
QUICK="${1:-}"

mkdir -p "$LOG_DIR" "$HOME/bin"

G='\033[32m'; R='\033[31m'; Y='\033[33m'; D='\033[2m'; N='\033[0m'
step=0

ok()   { step=$((step+1)); printf "  [%d] %s ${G}OK${N}\n" "$step" "$*"; }
warn() { step=$((step+1)); printf "  [%d] %s ${Y}WARN${N}\n" "$step" "$*"; }
fail() { step=$((step+1)); printf "  [%d] %s ${R}FAIL${N}\n" "$step" "$*"; exit 1; }

echo ""
echo "============================================"
echo "  h-uman deploy"
echo "============================================"
echo ""

# ── 1. Kill ALL existing service processes ────────────────────────────
# Unload every human-related service plist to prevent respawn
for plist in "$HOME"/Library/LaunchAgents/*human*service*.plist; do
    [ -f "$plist" ] && launchctl unload "$plist" 2>/dev/null || true
done
pkill -f "human service-loop" 2>/dev/null || true
pkill -f "human service_loop" 2>/dev/null || true
sleep 1
REMAINING=$(ps aux | grep -c -E "[h]uman.*(service-loop|service_loop)" || true)
if [ "$REMAINING" -eq 0 ]; then
    ok "Stop all services"
else
    # Force kill
    pkill -9 -f "human service-loop" 2>/dev/null || true
    sleep 1
    ok "Stop all services (force killed)"
fi

# ── 2. Remove duplicate plists ───────────────────────────────────────
REMOVED=0
for plist in "$HOME"/Library/LaunchAgents/*human*service*.plist; do
    if [ -f "$plist" ]; then
        PLABEL="$(defaults read "$plist" Label 2>/dev/null || true)"
        if [ -n "$PLABEL" ] && [ "$PLABEL" != "$PLIST_LABEL" ]; then
            rm "$plist"
            REMOVED=$((REMOVED + 1))
        fi
    fi
done
ok "Deduplicate plists ($REMOVED stale removed, keeping $PLIST_LABEL)"

# ── 3. Build ──────────────────────────────────────────────────────────
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DHU_ENABLE_SQLITE=ON \
    -DHU_ENABLE_ML=ON \
    -DHU_ENABLE_CURL=ON \
    > /dev/null 2>&1
cmake --build "$BUILD_DIR" -j"$JOBS" > /dev/null 2>&1
ok "Build ($(du -sh "$BUILD_DIR/human" | awk '{print $1}'))"

# ── 4. Tests (unless --quick) ─────────────────────────────────────────
if [ "$QUICK" != "--quick" ]; then
    PROVE_OUT=$("$BUILD_DIR/human_tests" --suite=prove_e2e 2>&1) || true
    PROVE_RESULT=$(echo "$PROVE_OUT" | grep -oE '[0-9]+/[0-9]+ passed' | tail -1)
    if echo "$PROVE_OUT" | grep -qE "[1-9][0-9]* FAILED"; then
        fail "Proof tests ($PROVE_RESULT)"
    else
        ok "Proof tests ($PROVE_RESULT)"
    fi

    EVAL_OUT=$(bash "$REPO/scripts/eval-conversations.sh" 2>&1) || true
    AVG=$(echo "$EVAL_OUT" | grep "Average quality" | grep -oE '[0-9]+' | head -1)
    EVAL_PASS=$(echo "$EVAL_OUT" | grep -c "PASS" || echo 0)
    if [ "${AVG:-0}" -ge 60 ]; then
        ok "Conversation eval ($EVAL_PASS/10 pass, ${AVG}% avg)"
    else
        warn "Conversation eval ($EVAL_PASS/10 pass, ${AVG}% avg)"
    fi
fi

# ── 5. Install binary ─────────────────────────────────────────────────
[ -f "$INSTALL_PATH" ] && cp "$INSTALL_PATH" "$INSTALL_PATH.bak"
cp "$BUILD_DIR/human" "$INSTALL_PATH"
chmod +x "$INSTALL_PATH"

APP_BUNDLE="$HOME/Applications/Human.app"
if [ -d "$APP_BUNDLE" ]; then
    cp "$BUILD_DIR/human" "$APP_BUNDLE/Contents/MacOS/human-service"
    chmod +x "$APP_BUNDLE/Contents/MacOS/human-service"
    codesign --force --deep --sign - --identifier ai.human.service "$APP_BUNDLE" 2>/dev/null || true
fi
ok "Install ($INSTALL_PATH)"

# ── 6. Permission checks ─────────────────────────────────────────────
PERMS=""
PERMS_OK=true

if sqlite3 ~/Library/Messages/chat.db "SELECT 1" > /dev/null 2>&1; then
    PERMS="${PERMS}FDA:ok "
else
    PERMS="${PERMS}FDA:MISSING "
    PERMS_OK=false
fi

if osascript -e 'tell application "System Events" to get name of first process' > /dev/null 2>&1; then
    PERMS="${PERMS}Accessibility:ok "
else
    PERMS="${PERMS}Accessibility:MISSING "
    PERMS_OK=false
fi

if osascript -e 'tell application "Messages" to name' > /dev/null 2>&1; then
    PERMS="${PERMS}Messages:ok"
else
    PERMS="${PERMS}Messages:MISSING"
    PERMS_OK=false
fi

if [ "$PERMS_OK" = true ]; then
    ok "Permissions ($PERMS)"
else
    warn "Permissions ($PERMS)"
    echo "    → System Settings > Privacy & Security > Full Disk Access"
    echo "    → Add: Terminal.app AND $INSTALL_PATH"
    echo "    → System Settings > Privacy & Security > Accessibility"
    echo "    → Add: Terminal.app AND $INSTALL_PATH"
fi

# ── 7. Sync rowid ─────────────────────────────────────────────────────
MAXID=$(sqlite3 ~/Library/Messages/chat.db "SELECT MAX(ROWID) FROM message" 2>/dev/null || echo "")
if [ -n "$MAXID" ]; then
    echo "$MAXID" > ~/.human/imessage.rowid
    ok "Sync rowid ($MAXID)"
else
    warn "Could not read iMessage rowid"
fi

# ── 8. Start service ─────────────────────────────────────────────────
if [ -f "$PLIST_PATH" ]; then
    launchctl load "$PLIST_PATH" 2>/dev/null || \
        launchctl bootstrap "gui/$(id -u)" "$PLIST_PATH" 2>/dev/null || true
    sleep 3
    SVC_PID=$(launchctl list 2>/dev/null | grep "$PLIST_LABEL" | awk '{print $1}')
    if [ -n "$SVC_PID" ] && [ "$SVC_PID" != "-" ]; then
        ok "Service started (PID $SVC_PID, launchd auto-restart)"
    else
        warn "Service loaded but PID unclear — check: launchctl list | grep human"
    fi
else
    warn "No launchd plist at $PLIST_PATH — run manually: human service-loop"
fi

# ── Summary ───────────────────────────────────────────────────────────
echo ""
echo "============================================"
printf "  ${G}Deploy complete.${N}\n"
echo ""
echo "  Status:   bash scripts/start-service.sh --status"
echo "  Logs:     tail -f $LOG_DIR/service-loop.log"
echo "  Stop:     launchctl unload $PLIST_PATH"
echo "  Redeploy: bash scripts/deploy.sh"
echo "============================================"
echo ""
