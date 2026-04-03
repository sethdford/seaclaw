#!/usr/bin/env bash
# start-service.sh — Start h-uman service loop with health monitoring.
#
# Usage: bash scripts/start-service.sh          # foreground
#        bash scripts/start-service.sh --bg     # background (writes PID file)
#        bash scripts/start-service.sh --stop   # stop background service
#        bash scripts/start-service.sh --status  # check if running
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
cd "$ROOT"

BUILD="${BUILD:-build}"
BINARY="$BUILD/human"
PID_FILE="$HOME/.human/service.pid"
LOG_FILE="$HOME/.human/logs/service-loop.log"

if [ ! -x "$BINARY" ]; then
    echo "Error: $BINARY not found. Run: cmake --build build" >&2
    exit 1
fi

case "${1:-}" in
    --stop)
        if [ -f "$PID_FILE" ]; then
            PID=$(cat "$PID_FILE")
            if kill -0 "$PID" 2>/dev/null; then
                echo "Stopping h-uman service (PID $PID)..."
                kill "$PID"
                rm -f "$PID_FILE"
                echo "Stopped."
            else
                echo "Service not running (stale PID file). Cleaning up."
                rm -f "$PID_FILE"
            fi
        else
            echo "No service running (no PID file)."
        fi
        exit 0
        ;;
    --status)
        if [ -f "$PID_FILE" ]; then
            PID=$(cat "$PID_FILE")
            if kill -0 "$PID" 2>/dev/null; then
                echo "h-uman service is RUNNING (PID $PID)"
                echo "  Log: $LOG_FILE"
                echo "  Rowid: $(cat ~/.human/imessage.rowid 2>/dev/null || echo 'unknown')"
                echo "  DB max: $(sqlite3 ~/Library/Messages/chat.db 'SELECT MAX(ROWID) FROM message' 2>/dev/null || echo 'unknown')"
                tail -5 "$LOG_FILE" 2>/dev/null | sed 's/^/  /'
                exit 0
            else
                echo "h-uman service is NOT RUNNING (stale PID file)"
                rm -f "$PID_FILE"
                exit 1
            fi
        else
            echo "h-uman service is NOT RUNNING"
            exit 1
        fi
        ;;
    --bg)
        mkdir -p "$(dirname "$LOG_FILE")"
        echo "Starting h-uman service in background..."
        nohup "$BINARY" service-loop >> "$LOG_FILE" 2>&1 &
        echo $! > "$PID_FILE"
        sleep 2
        if kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
            echo "h-uman service started (PID $(cat "$PID_FILE"))"
            echo "  Log: $LOG_FILE"
            echo "  Stop: bash scripts/start-service.sh --stop"
            echo "  Status: bash scripts/start-service.sh --status"
            tail -5 "$LOG_FILE" 2>/dev/null | sed 's/^/  /'
        else
            echo "ERROR: Service failed to start. Check $LOG_FILE"
            tail -20 "$LOG_FILE" 2>/dev/null
            exit 1
        fi
        ;;
    *)
        # Foreground mode
        echo ""
        echo "============================================"
        echo "  h-uman service (foreground)"
        echo "============================================"
        echo "  Binary: $BINARY"
        echo "  Config: ~/.human/config.json"
        echo "  Memory: ~/.human/memory.db"
        echo "  Persona: seth"
        echo "  Channels: iMessage"
        echo "  Press Ctrl+C to stop"
        echo "============================================"
        echo ""
        exec "$BINARY" service-loop
        ;;
esac
