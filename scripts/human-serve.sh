#!/usr/bin/env bash
# human-serve.sh — Manage the local MLX model server for h-uman.
#
# Usage:
#   human-serve.sh start    # Start the server (background)
#   human-serve.sh stop     # Stop the server
#   human-serve.sh status   # Check if running
#   human-serve.sh restart  # Stop + start
#   human-serve.sh ensure   # Start only if not already running (for auto-start)
#
# Reads config from ~/.human/config.json for model/adapter/port.
# Falls back to sensible defaults if config is missing.

set -euo pipefail

CONFIG="$HOME/.human/config.json"
PIDFILE="$HOME/.human/mlx-server.pid"
LOGFILE="$HOME/.human/mlx-server.log"

DEFAULT_MODEL="mlx-community/gemma-4-26b-a4b-it-4bit"
DEFAULT_ADAPTER="$HOME/.human/adapters/persona"
DEFAULT_PORT=8741

read_config() {
    if [[ -f "$CONFIG" ]] && command -v python3 &>/dev/null; then
        MODEL=$(python3 -c "
import json, os
try:
    with open('$CONFIG') as f:
        c = json.load(f)
    mlx = c.get('mlx_local', {})
    print(mlx.get('model', c.get('default_model', '$DEFAULT_MODEL')))
except: print('$DEFAULT_MODEL')
" 2>/dev/null)
        ADAPTER=$(python3 -c "
import json, os
try:
    with open('$CONFIG') as f:
        c = json.load(f)
    p = c.get('mlx_local', {}).get('adapter_path', '$DEFAULT_ADAPTER')
    print(os.path.expanduser(p))
except: print('$DEFAULT_ADAPTER')
" 2>/dev/null)
        PORT=$(python3 -c "
import json
try:
    with open('$CONFIG') as f:
        c = json.load(f)
    print(c.get('mlx_local', {}).get('port', $DEFAULT_PORT))
except: print($DEFAULT_PORT)
" 2>/dev/null)
    else
        MODEL="$DEFAULT_MODEL"
        ADAPTER="$DEFAULT_ADAPTER"
        PORT="$DEFAULT_PORT"
    fi
}

is_running() {
    if [[ -f "$PIDFILE" ]]; then
        local pid
        pid=$(cat "$PIDFILE" 2>/dev/null)
        if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
            return 0
        fi
    fi
    # Also check by port
    if lsof -ti:"$PORT" &>/dev/null; then
        return 0
    fi
    return 1
}

do_start() {
    read_config
    if is_running; then
        echo "MLX server already running on port $PORT"
        return 0
    fi

    echo "Starting MLX model server..."
    echo "  Model:   $MODEL"
    echo "  Adapter: $ADAPTER"
    echo "  Port:    $PORT"
    echo "  Log:     $LOGFILE"

    local turbo_script
    turbo_script="$(cd "$(dirname "$0")" && pwd)/turbo-serve.py"

    local cmd
    if [[ -f "$turbo_script" ]]; then
        cmd="python3 $turbo_script --model $MODEL --port $PORT"
        echo "  Turbo:   KV cache compression enabled"
    else
        cmd="python3 -m mlx_lm.server --model $MODEL --port $PORT"
    fi
    if [[ -d "$ADAPTER" ]] && [[ -f "$ADAPTER/adapters.safetensors" ]]; then
        cmd="$cmd --adapter-path $ADAPTER"
        echo "  LoRA:    persona adapter loaded"
    fi

    nohup $cmd > "$LOGFILE" 2>&1 &
    local pid=$!
    echo "$pid" > "$PIDFILE"

    # Wait for server to be ready
    echo -n "  Waiting for server"
    for i in $(seq 1 30); do
        if curl -sf "http://127.0.0.1:$PORT/v1/models" &>/dev/null; then
            echo " ready!"
            echo "  PID:     $pid"
            echo "  URL:     http://127.0.0.1:$PORT"
            return 0
        fi
        echo -n "."
        sleep 1
    done
    echo " timeout (server may still be loading model)"
    echo "  Check: tail -f $LOGFILE"
}

do_stop() {
    read_config
    if [[ -f "$PIDFILE" ]]; then
        local pid
        pid=$(cat "$PIDFILE" 2>/dev/null)
        if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
            echo "Stopping MLX server (PID $pid)..."
            kill "$pid" 2>/dev/null
            rm -f "$PIDFILE"
            echo "Stopped."
            return 0
        fi
    fi
    # Try by port
    local pids
    pids=$(lsof -ti:"$PORT" 2>/dev/null || true)
    if [[ -n "$pids" ]]; then
        echo "Stopping processes on port $PORT..."
        echo "$pids" | xargs kill 2>/dev/null
        rm -f "$PIDFILE"
        echo "Stopped."
    else
        echo "MLX server not running."
    fi
}

do_status() {
    read_config
    if is_running; then
        local pid
        pid=$(lsof -ti:"$PORT" 2>/dev/null | head -1)
        echo "MLX server running on port $PORT (PID: ${pid:-unknown})"
        echo "  Model:   $MODEL"
        if [[ -d "$ADAPTER" ]]; then
            echo "  Adapter: $ADAPTER"
        fi
    else
        echo "MLX server not running"
        return 1
    fi
}

do_ensure() {
    read_config
    if is_running; then
        return 0
    fi
    do_start
}

case "${1:-status}" in
    start)   do_start ;;
    stop)    do_stop ;;
    status)  do_status ;;
    restart) do_stop; sleep 2; do_start ;;
    ensure)  do_ensure ;;
    *)
        echo "Usage: $0 {start|stop|status|restart|ensure}"
        exit 1
        ;;
esac
