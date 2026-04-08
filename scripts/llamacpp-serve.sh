#!/usr/bin/env bash
# llama.cpp Metal server — raw C++ inference with fused Metal kernels.
#
# llama.cpp has Metal-optimized kernels that MLX lacks: fused RoPE+attention,
# flash attention v2, and optimized KV cache management. On Apple Silicon this
# can yield 50-80+ tok/s for Gemma 4 E4B with 4-bit quantization.
#
# Usage:
#   ./scripts/llamacpp-serve.sh                        # serve E4B (default)
#   ./scripts/llamacpp-serve.sh --model 31b            # serve 31B
#   ./scripts/llamacpp-serve.sh --gguf /path/to.gguf   # serve custom GGUF
#   ./scripts/llamacpp-serve.sh --port 8741            # custom port
#   ./scripts/llamacpp-serve.sh --build                # build from source first
#
# Prerequisites:
#   brew install llama.cpp   (or build from source with --build flag)
#
# The server exposes an OpenAI-compatible API at http://127.0.0.1:PORT/v1

set -euo pipefail

# ── Defaults ──────────────────────────────────────────────────────
MODEL_TARGET="e4b"
PORT="${LLAMACPP_PORT:-8742}"
GGUF_PATH=""
NUM_GPU=999
NUM_CTX=4096
FLASH_ATTN=1
REASONING_BUDGET=0   # 0 = disable thinking tokens (voice mode default)
THREADS=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 8)
BATCH_SIZE=512
UBATCH_SIZE=256
BUILD_FROM_SOURCE=0
LLAMA_DIR="${LLAMA_CPP_DIR:-$HOME/.local/llama.cpp}"

MODELS_DIR="${LLAMA_MODELS_DIR:-$HOME/.local/share/llama-models}"

resolve_gguf() {
    case "$1" in
        e4b) GGUF_REPO="ggml-org/gemma-4-E4B-it-GGUF"; GGUF_FILE="gemma-4-e4b-it-Q4_K_M.gguf" ;;
        e2b) GGUF_REPO="ggml-org/gemma-4-E2B-it-GGUF"; GGUF_FILE="gemma-4-e2b-it-Q4_K_M.gguf" ;;
        31b) GGUF_REPO="bartowski/google_gemma-4-27B-it-GGUF"; GGUF_FILE="gemma-4-27b-it-Q4_K_M.gguf" ;;
        *)   GGUF_REPO=""; GGUF_FILE="" ;;
    esac
}

# ── Parse args ───────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --model)      MODEL_TARGET="$2"; shift 2 ;;
        --port)       PORT="$2"; shift 2 ;;
        --gguf)       GGUF_PATH="$2"; shift 2 ;;
        --ctx)        NUM_CTX="$2"; shift 2 ;;
        --threads)    THREADS="$2"; shift 2 ;;
        --batch)      BATCH_SIZE="$2"; shift 2 ;;
        --think)      REASONING_BUDGET=2147483647; shift ;;
        --no-think)   REASONING_BUDGET=0; shift ;;
        --build)      BUILD_FROM_SOURCE=1; shift ;;
        --llama-dir)  LLAMA_DIR="$2"; shift 2 ;;
        --help|-h)
            echo "Usage: $0 [--model e4b|e2b|31b] [--port PORT] [--gguf PATH] [--build]"
            echo ""
            echo "Serve Gemma 4 via llama.cpp Metal server — fused kernels, flash attention."
            echo "Typically 50-90+ tok/s for E4B Q4_K_M on M4 Max."
            echo ""
            echo "Options:"
            echo "  --model      Model target: e4b (default), e2b, 31b"
            echo "  --port       Server port (default: 8742)"
            echo "  --gguf       Path to custom GGUF file"
            echo "  --ctx        Context window (default: 4096)"
            echo "  --threads    CPU threads (default: auto)"
            echo "  --batch      Batch size for prompt processing (default: 512)"
            echo "  --think      Enable thinking/reasoning tokens (off by default for voice)"
            echo "  --no-think   Disable thinking tokens (default)"
            echo "  --build      Build llama.cpp from source before serving"
            echo "  --llama-dir  llama.cpp install directory (default: ~/.local/llama.cpp)"
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ── Build from source (optional) ────────────────────────────────
build_llamacpp() {
    echo "  Building llama.cpp from source with Metal support..."
    if [[ -d "$LLAMA_DIR" ]]; then
        cd "$LLAMA_DIR" && git pull
    else
        git clone https://github.com/ggerganov/llama.cpp.git "$LLAMA_DIR"
        cd "$LLAMA_DIR"
    fi

    mkdir -p build && cd build
    cmake .. \
        -DGGML_METAL=ON \
        -DGGML_METAL_EMBED_LIBRARY=ON \
        -DLLAMA_CURL=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DGGML_METAL_SHADER_DEBUG=OFF
    cmake --build . --config Release -j"$(sysctl -n hw.logicalcpu)"
    echo "  Build complete: $LLAMA_DIR/build/bin/llama-server"
}

if [[ "$BUILD_FROM_SOURCE" -eq 1 ]]; then
    build_llamacpp
fi

# ── Find llama-server binary ─────────────────────────────────────
LLAMA_SERVER=""
for candidate in \
    "$LLAMA_DIR/build/bin/llama-server" \
    "$(command -v llama-server 2>/dev/null || true)" \
    "$(brew --prefix 2>/dev/null)/bin/llama-server" \
    "$HOME/.local/bin/llama-server"; do
    if [[ -n "$candidate" && -x "$candidate" ]]; then
        LLAMA_SERVER="$candidate"
        break
    fi
done

if [[ -z "$LLAMA_SERVER" ]]; then
    echo "ERROR: llama-server not found. Install with:"
    echo "  brew install llama.cpp"
    echo "  OR: $0 --build  (build from source with Metal)"
    exit 1
fi

echo ""
echo "============================================================"
echo "  llama.cpp Metal Server"
echo "============================================================"

CHIP=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "unknown")
MEM_BYTES=$(sysctl -n hw.memsize 2>/dev/null || echo "0")
MEM_GB=$((MEM_BYTES / 1073741824))
echo "  Hardware:  $CHIP"
echo "  Memory:    ${MEM_GB} GB unified"
echo "  Binary:    $LLAMA_SERVER"
echo "  Threads:   $THREADS"

# ── Resolve GGUF model path ─────────────────────────────────────
if [[ -z "$GGUF_PATH" ]]; then
    resolve_gguf "$MODEL_TARGET"

    if [[ -z "$GGUF_FILE" ]]; then
        echo "ERROR: Unknown model target '$MODEL_TARGET'"
        exit 1
    fi

    GGUF_PATH="$MODELS_DIR/$GGUF_FILE"

    if [[ ! -f "$GGUF_PATH" ]]; then
        echo "  Model not found locally, downloading..."
        mkdir -p "$MODELS_DIR"

        if command -v huggingface-cli &>/dev/null; then
            huggingface-cli download "$GGUF_REPO" "$GGUF_FILE" \
                --local-dir "$MODELS_DIR" --local-dir-use-symlinks False
        else
            HF_URL="https://huggingface.co/$GGUF_REPO/resolve/main/$GGUF_FILE"
            echo "  Downloading from $HF_URL ..."
            curl -L -o "$GGUF_PATH" "$HF_URL"
        fi
    fi
fi

if [[ ! -f "$GGUF_PATH" ]]; then
    echo "ERROR: GGUF file not found: $GGUF_PATH"
    exit 1
fi

GGUF_SIZE=$(du -h "$GGUF_PATH" | cut -f1)
echo "  Model:     $MODEL_TARGET ($GGUF_PATH)"
echo "  Size:      $GGUF_SIZE"
echo "  Context:   $NUM_CTX tokens"
echo "  Port:      $PORT"
echo "  Flash attn: $([ $FLASH_ATTN -eq 1 ] && echo 'YES' || echo 'no')"
echo "  Thinking:   $([ $REASONING_BUDGET -gt 0 ] && echo "ON (budget=$REASONING_BUDGET)" || echo 'OFF (voice mode)')"
echo "============================================================"
echo ""
echo "  Endpoint: http://127.0.0.1:$PORT/v1/chat/completions"
echo ""
echo "  Test:"
echo "    curl http://127.0.0.1:$PORT/v1/chat/completions \\"
echo "      -H 'Content-Type: application/json' \\"
echo "      -d '{\"messages\": [{\"role\": \"user\", \"content\": \"hey\"}]}'"
echo ""
echo "  Benchmark:"
echo "    python3 scripts/voice-bench.py --endpoint http://127.0.0.1:$PORT"
echo ""
echo "  Press Ctrl+C to stop."
echo ""

# ── Launch server ─────────────────────────────────────────────────
EXTRA_ARGS=()
if [[ "$FLASH_ATTN" -eq 1 ]]; then
    EXTRA_ARGS+=("--flash-attn" "on")
fi
EXTRA_ARGS+=("--reasoning-budget" "$REASONING_BUDGET")

exec "$LLAMA_SERVER" \
    --model "$GGUF_PATH" \
    --port "$PORT" \
    --host "127.0.0.1" \
    --ctx-size "$NUM_CTX" \
    --threads "$THREADS" \
    --n-gpu-layers "$NUM_GPU" \
    --batch-size "$BATCH_SIZE" \
    --ubatch-size "$UBATCH_SIZE" \
    --parallel 1 \
    --cont-batching \
    --mlock \
    --no-mmap \
    "${EXTRA_ARGS[@]}"
