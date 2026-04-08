#!/usr/bin/env bash
# vLLM Metal server — production-grade serving with paged attention on Apple Silicon.
#
# vLLM provides continuous batching, paged attention (PagedAttention v2), and
# chunked prefill — all running on Metal via the MLX backend. This is the
# closest thing to a production GPU cluster on a Mac.
#
# Key advantages over raw MLX:
#   - Paged attention: efficient KV cache management, no wasted memory
#   - Continuous batching: multiple requests processed concurrently
#   - Chunked prefill: long prompts processed incrementally (no TTFT spike)
#   - Prefix caching: shared system prompt KV across requests
#   - OpenAI-compatible API with proper usage tracking
#
# Usage:
#   ./scripts/vllm-metal-serve.sh                    # serve E4B (default)
#   ./scripts/vllm-metal-serve.sh --model 31b        # serve 31B
#   ./scripts/vllm-metal-serve.sh --port 8741        # custom port
#   ./scripts/vllm-metal-serve.sh --speculative e2b  # speculative decoding
#
# Prerequisites:
#   pip install vllm   (requires vLLM >=0.8 with Metal support)
#
# The server exposes an OpenAI-compatible API at http://127.0.0.1:PORT/v1

set -euo pipefail

# ── Defaults ──────────────────────────────────────────────────────
MODEL_TARGET="e4b"
PORT="${VLLM_PORT:-8741}"
SPECULATIVE_MODEL=""
MAX_MODEL_LEN=4096
GPU_MEMORY_UTILIZATION=0.9
ENABLE_PREFIX_CACHE=1
ENABLE_CHUNKED_PREFILL=1
MAX_NUM_SEQS=4

# ── Model registry ──────────────────────────────────────────────
resolve_vllm_model() {
    case "$1" in
        e4b) VLLM_MODEL="google/gemma-4-e4b-it" ;;
        e2b) VLLM_MODEL="google/gemma-4-e2b-it" ;;
        31b) VLLM_MODEL="google/gemma-4-31b-it" ;;
        *)   VLLM_MODEL="" ;;
    esac
}

# ── Parse args ───────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --model)          MODEL_TARGET="$2"; shift 2 ;;
        --port)           PORT="$2"; shift 2 ;;
        --speculative)    SPECULATIVE_MODEL="$2"; shift 2 ;;
        --max-model-len)  MAX_MODEL_LEN="$2"; shift 2 ;;
        --max-seqs)       MAX_NUM_SEQS="$2"; shift 2 ;;
        --no-prefix-cache) ENABLE_PREFIX_CACHE=0; shift ;;
        --no-chunked-prefill) ENABLE_CHUNKED_PREFILL=0; shift ;;
        --help|-h)
            echo "Usage: $0 [--model e4b|e2b|31b] [--port PORT] [--speculative e2b]"
            echo ""
            echo "Serve Gemma 4 via vLLM Metal — paged attention, continuous batching."
            echo ""
            echo "Options:"
            echo "  --model             Model target: e4b (default), e2b, 31b"
            echo "  --port              Server port (default: 8741)"
            echo "  --speculative       Speculative decoding draft model (e2b)"
            echo "  --max-model-len     Max context length (default: 4096)"
            echo "  --max-seqs          Max concurrent sequences (default: 4)"
            echo "  --no-prefix-cache   Disable prefix/system-prompt caching"
            echo "  --no-chunked-prefill  Disable chunked prefill"
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ── Preflight ────────────────────────────────────────────────────
if ! python3 -c "import vllm" 2>/dev/null; then
    echo "ERROR: vLLM not installed. Install with:"
    echo "  pip install vllm"
    echo ""
    echo "Note: vLLM Metal support requires vLLM >=0.8"
    echo "Check: https://docs.vllm.ai/en/latest/getting_started/installation/apple.html"
    exit 1
fi

VLLM_VERSION=$(python3 -c "import vllm; print(vllm.__version__)" 2>/dev/null || echo "unknown")
resolve_vllm_model "$MODEL_TARGET"
if [[ -z "$VLLM_MODEL" ]]; then
    echo "ERROR: Unknown model target '$MODEL_TARGET'"
    exit 1
fi

echo ""
echo "============================================================"
echo "  vLLM Metal Server (Paged Attention + Continuous Batching)"
echo "============================================================"

CHIP=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "unknown")
MEM_BYTES=$(sysctl -n hw.memsize 2>/dev/null || echo "0")
MEM_GB=$((MEM_BYTES / 1073741824))
echo "  Hardware:    $CHIP"
echo "  Memory:      ${MEM_GB} GB unified"
echo "  vLLM:        v${VLLM_VERSION}"
echo "  Model:       $MODEL_TARGET ($VLLM_MODEL)"
echo "  Context:     $MAX_MODEL_LEN tokens"
echo "  Max seqs:    $MAX_NUM_SEQS (continuous batching)"
echo "  Prefix cache: $([ $ENABLE_PREFIX_CACHE -eq 1 ] && echo 'YES' || echo 'no')"
echo "  Chunked prefill: $([ $ENABLE_CHUNKED_PREFILL -eq 1 ] && echo 'YES' || echo 'no')"
if [[ -n "$SPECULATIVE_MODEL" ]]; then
    SAVED_MODEL="$VLLM_MODEL"
    resolve_vllm_model "$SPECULATIVE_MODEL"
    SPEC_HF="${VLLM_MODEL:-$SPECULATIVE_MODEL}"
    VLLM_MODEL="$SAVED_MODEL"
    echo "  Speculative: $SPEC_HF"
fi
echo "  Port:        $PORT"
echo "============================================================"
echo ""

# ── Build args ────────────────────────────────────────────────────
VLLM_ARGS=(
    "--model" "$VLLM_MODEL"
    "--port" "$PORT"
    "--host" "127.0.0.1"
    "--max-model-len" "$MAX_MODEL_LEN"
    "--gpu-memory-utilization" "$GPU_MEMORY_UTILIZATION"
    "--max-num-seqs" "$MAX_NUM_SEQS"
    "--dtype" "auto"
    "--trust-remote-code"
)

if [[ "$ENABLE_PREFIX_CACHE" -eq 1 ]]; then
    VLLM_ARGS+=("--enable-prefix-caching")
fi

if [[ "$ENABLE_CHUNKED_PREFILL" -eq 1 ]]; then
    VLLM_ARGS+=("--enable-chunked-prefill")
fi

if [[ -n "$SPECULATIVE_MODEL" ]]; then
    SAVED_VLLM="$VLLM_MODEL"
    resolve_vllm_model "$SPECULATIVE_MODEL"
    SPEC_HF="${VLLM_MODEL:-$SPECULATIVE_MODEL}"
    VLLM_MODEL="$SAVED_VLLM"
    VLLM_ARGS+=(
        "--speculative-model" "$SPEC_HF"
        "--num-speculative-tokens" "4"
        "--speculative-disable-mqa-scorer"
    )
fi

echo "  Endpoint: http://127.0.0.1:$PORT/v1/chat/completions"
echo ""
echo "  Test:"
echo "    curl http://127.0.0.1:$PORT/v1/chat/completions \\"
echo "      -H 'Content-Type: application/json' \\"
echo "      -d '{\"model\": \"$VLLM_MODEL\", \"messages\": [{\"role\": \"user\", \"content\": \"hey\"}]}'"
echo ""
echo "  Benchmark:"
echo "    python3 scripts/voice-bench.py --endpoint http://127.0.0.1:$PORT"
echo ""
echo "  Press Ctrl+C to stop."
echo ""

exec python3 -m vllm.entrypoints.openai.api_server "${VLLM_ARGS[@]}"
