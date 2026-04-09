#!/usr/bin/env bash
# Install a macOS LaunchAgent that auto-starts the MLX model server on login.
# Only runs on macOS. Harmless no-op on Linux.

set -euo pipefail

if [[ "$(uname)" != "Darwin" ]]; then
    echo "LaunchAgent is macOS-only. On Linux, use a systemd user service."
    exit 0
fi

PLIST_DIR="$HOME/Library/LaunchAgents"
PLIST="$PLIST_DIR/com.h-uman.mlx-server.plist"
SERVE_SCRIPT="$(cd "$(dirname "$0")" && pwd)/human-serve.sh"

if [[ ! -f "$SERVE_SCRIPT" ]]; then
    echo "Error: human-serve.sh not found at $SERVE_SCRIPT"
    exit 1
fi

mkdir -p "$PLIST_DIR"

cat > "$PLIST" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.h-uman.mlx-server</string>
    <key>ProgramArguments</key>
    <array>
        <string>/bin/bash</string>
        <string>$SERVE_SCRIPT</string>
        <string>ensure</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <false/>
    <key>StandardOutPath</key>
    <string>$HOME/.human/mlx-launchd.log</string>
    <key>StandardErrorPath</key>
    <string>$HOME/.human/mlx-launchd.log</string>
</dict>
</plist>
PLIST

echo "LaunchAgent installed: $PLIST"
echo "Loading..."

launchctl unload "$PLIST" 2>/dev/null || true
launchctl load "$PLIST"

echo "Done. Server will auto-start on login."
echo "Manual control: scripts/human-serve.sh {start|stop|status}"
