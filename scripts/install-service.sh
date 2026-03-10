#!/usr/bin/env bash
# Install seaclaw LaunchAgent for auto-start on login.
# Run from the nullclaw repo root or scripts/ directory.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLIST_SRC="${SCRIPT_DIR}/com.seaclaw.service.plist"
PLIST_DEST="${HOME}/Library/LaunchAgents/com.seaclaw.service.plist"

# Ensure ~/.seaclaw exists for log files
mkdir -p "${HOME}/.seaclaw"

cp "$PLIST_SRC" "$PLIST_DEST"
launchctl load "$PLIST_DEST"

echo "seaclaw LaunchAgent installed successfully."
echo "  Plist: $PLIST_DEST"
echo "  Logs:  ${HOME}/.seaclaw/seaclaw.log"
echo ""
echo "To unload: launchctl unload ~/Library/LaunchAgents/com.seaclaw.service.plist"
