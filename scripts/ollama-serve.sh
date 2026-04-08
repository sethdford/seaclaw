#!/usr/bin/env bash
# Ollama-based model serving for Gemma 4 — bypasses Python GIL for ~4x speed.
#
# Ollama uses a Go HTTP server + llama.cpp Metal backend. No Python in the
# hot path means no GIL serialization, no numpy↔MLX sync, pure GPU dispatch.
#
# Usage:
#   ./scripts/ollama-serve.sh                    # serve Gemma 4 E4B (default)
#   ./scripts/ollama-serve.sh --model e2b        # serve E2B draft model
#   ./scripts/ollama-serve.sh --model 31b        # serve 31B dense
#   ./scripts/ollama-serve.sh --adapter ~/path   # apply LoRA adapter via GGUF merge
#   ./scripts/ollama-serve.sh --port 8741        # custom port (default: 11434)
#
# Prerequisites:
#   brew install ollama   OR   curl -fsSL https://ollama.com/install.sh | sh
#
# The server exposes an OpenAI-compatible API at http://127.0.0.1:PORT/v1

set -euo pipefail

# ── Defaults ──────────────────────────────────────────────────────
MODEL_TARGET="e4b"
PORT="${OLLAMA_PORT:-11434}"
ADAPTER_PATH=""
NUM_GPU=999          # use all GPU layers (Metal)
NUM_CTX=4096
FLASH_ATTN=1
NUM_PARALLEL=1
VERBOSE=0

# ── Model registry ───────────────────────────────────────────────
resolve_ollama_model() {
    case "$1" in
        e4b) OLLAMA_MODEL="gemma4:4b";  OLLAMA_FALLBACK="gemma3:4b" ;;
        e2b) OLLAMA_MODEL="gemma3:2b";  OLLAMA_FALLBACK="gemma3:2b" ;;
        31b) OLLAMA_MODEL="gemma4:26b"; OLLAMA_FALLBACK="gemma3:27b" ;;
        *)   OLLAMA_MODEL=""; OLLAMA_FALLBACK="" ;;
    esac
}

# ── Parse args ───────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --model)     MODEL_TARGET="$2"; shift 2 ;;
        --port)      PORT="$2"; shift 2 ;;
        --adapter)   ADAPTER_PATH="$2"; shift 2 ;;
        --ctx)       NUM_CTX="$2"; shift 2 ;;
        --parallel)  NUM_PARALLEL="$2"; shift 2 ;;
        --verbose)   VERBOSE=1; shift ;;
        --help|-h)
            echo "Usage: $0 [--model e4b|e2b|31b] [--port PORT] [--adapter PATH] [--ctx N]"
            echo ""
            echo "Serve Gemma 4 models via Ollama (Go + llama.cpp Metal backend)."
            echo "~4x faster than Python MLX server due to zero GIL overhead."
            echo ""
            echo "Options:"
            echo "  --model     Model target: e4b (default), e2b, 31b"
            echo "  --port      Server port (default: 11434)"
            echo "  --adapter   Path to LoRA adapter (will merge into GGUF)"
            echo "  --ctx       Context window size (default: 4096)"
            echo "  --parallel  Number of parallel request slots (default: 1)"
            echo "  --verbose   Enable verbose logging"
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ── Preflight ────────────────────────────────────────────────────
if ! command -v ollama &>/dev/null; then
    echo "ERROR: ollama not found. Install with:"
    echo "  brew install ollama"
    echo "  OR: curl -fsSL https://ollama.com/install.sh | sh"
    exit 1
fi

resolve_ollama_model "$MODEL_TARGET"
if [[ -z "$OLLAMA_MODEL" ]]; then
    echo "ERROR: Unknown model target '$MODEL_TARGET'. Use: e4b, e2b, 31b"
    exit 1
fi

echo ""
echo "============================================================"
echo "  Ollama Inference Server (Go + llama.cpp Metal)"
echo "============================================================"

# ── Hardware info ────────────────────────────────────────────────
CHIP=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "unknown")
MEM_BYTES=$(sysctl -n hw.memsize 2>/dev/null || echo "0")
MEM_GB=$((MEM_BYTES / 1073741824))
echo "  Hardware:  $CHIP"
echo "  Memory:    ${MEM_GB} GB unified"
echo "  Model:     $MODEL_TARGET ($OLLAMA_MODEL)"
echo "  Context:   $NUM_CTX tokens"
echo "  Port:      $PORT"
echo "  Backend:   llama.cpp Metal (zero Python overhead)"

# ── Handle LoRA adapter merging ──────────────────────────────────
CUSTOM_MODEL=""
if [[ -n "$ADAPTER_PATH" ]]; then
    if [[ ! -d "$ADAPTER_PATH" ]]; then
        echo "ERROR: Adapter path not found: $ADAPTER_PATH"
        exit 1
    fi

    CUSTOM_MODEL="human-${MODEL_TARGET}-lora"
    echo "  Adapter:   $ADAPTER_PATH"
    echo ""
    echo "  Creating custom model with LoRA adapter..."

    MODELFILE=$(mktemp)
    cat > "$MODELFILE" <<MODELFILE_EOF
FROM $OLLAMA_MODEL
ADAPTER $ADAPTER_PATH
PARAMETER num_ctx $NUM_CTX
PARAMETER num_gpu $NUM_GPU
PARAMETER flash_attention $FLASH_ATTN
SYSTEM "You are a conversational assistant. Respond naturally and concisely."
MODELFILE_EOF

    ollama create "$CUSTOM_MODEL" -f "$MODELFILE"
    rm -f "$MODELFILE"
    OLLAMA_MODEL="$CUSTOM_MODEL"
    echo "  Custom model created: $CUSTOM_MODEL"
fi

echo "============================================================"
echo ""

# ── Pull model if needed (with fallback) ─────────────────────────
echo "  Checking model availability..."
if ! ollama show "$OLLAMA_MODEL" &>/dev/null 2>&1; then
    echo "  Pulling $OLLAMA_MODEL..."
    if ! ollama pull "$OLLAMA_MODEL" 2>/dev/null; then
        FALLBACK="$OLLAMA_FALLBACK"
        if [[ -n "$FALLBACK" && "$FALLBACK" != "$OLLAMA_MODEL" ]]; then
            echo "  Primary model unavailable, falling back to $FALLBACK..."
            OLLAMA_MODEL="$FALLBACK"
            if ! ollama show "$OLLAMA_MODEL" &>/dev/null 2>&1; then
                ollama pull "$OLLAMA_MODEL"
            fi
        else
            echo "ERROR: Could not pull model $OLLAMA_MODEL"
            exit 1
        fi
    fi
fi
echo "  Model ready: $OLLAMA_MODEL"
echo ""

# ── Start server ─────────────────────────────────────────────────
export OLLAMA_HOST="127.0.0.1:$PORT"
export OLLAMA_NUM_PARALLEL="$NUM_PARALLEL"
export OLLAMA_FLASH_ATTENTION="$FLASH_ATTN"
export OLLAMA_MAX_LOADED_MODELS=1
export OLLAMA_KEEP_ALIVE="24h"

if [[ "$VERBOSE" -eq 1 ]]; then
    export OLLAMA_DEBUG=1
fi

echo "  Starting Ollama server on http://127.0.0.1:$PORT ..."
echo "  OpenAI-compatible: http://127.0.0.1:$PORT/v1/chat/completions"
echo ""
echo "  Test:"
echo "    curl http://127.0.0.1:$PORT/v1/chat/completions \\"
echo "      -H 'Content-Type: application/json' \\"
echo "      -d '{\"model\": \"$OLLAMA_MODEL\", \"messages\": [{\"role\": \"user\", \"content\": \"hey\"}]}'"
echo ""
echo "  Benchmark:"
echo "    python3 scripts/voice-bench.py --endpoint http://127.0.0.1:$PORT"
echo ""
echo "  Press Ctrl+C to stop."
echo ""

# Pre-load the model into GPU memory before accepting requests
ollama run "$OLLAMA_MODEL" "" </dev/null 2>/dev/null || true

# Ollama's serve command starts the HTTP server
exec ollama serve
