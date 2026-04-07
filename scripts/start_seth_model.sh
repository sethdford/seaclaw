#!/bin/bash
# Start the fine-tuned Seth model server (MLX, OpenAI-compatible API)
#
# Uses Gemma 4 31B 4-bit with LoRA adapter from the fine-tuning pipeline.
# Supports TurboQuant KV cache compression and cross-turn prompt caching.
#
# Usage:
#   ./scripts/start_seth_model.sh [port]
#   ./scripts/start_seth_model.sh 8741 --kv-bits 3    # with TurboQuant

set -euo pipefail

PORT="${1:-8741}"
shift 2>/dev/null || true

MODEL="${MLX_MODEL:-mlx-community/gemma-4-31b-it-4bit}"
ADAPTER_DIR="${MLX_ADAPTER_PATH:-$HOME/.human/training-data/adapters/seth-lora}"

if ! command -v python3 &>/dev/null; then
    echo "Error: python3 not found"
    exit 1
fi

python3 -c "import mlx_vlm" 2>/dev/null || {
    echo "Error: mlx_vlm not installed. Run: pip3 install mlx-vlm"
    exit 1
}

ADAPTER_ARGS=""
if [ -d "$ADAPTER_DIR" ] && [ -f "$ADAPTER_DIR/adapters.safetensors" ]; then
    ADAPTER_ARGS="--adapter-path $ADAPTER_DIR"
    echo "LoRA adapter: $ADAPTER_DIR"
else
    echo "Warning: No LoRA adapter found at $ADAPTER_DIR"
    echo "  Run the fine-tuning pipeline first:"
    echo "    python3 scripts/prepare-finetune.py --persona seth --include-chatdb"
    echo "    python3 scripts/finetune-gemma.py"
    echo "  Starting base model without adapter..."
fi

echo "Starting Seth model server on port $PORT..."
echo "Model: $MODEL"
echo "API: http://localhost:$PORT/v1"
echo ""
exec python3 scripts/mlx-server.py \
    --model "$MODEL" \
    --port "$PORT" \
    $ADAPTER_ARGS \
    "$@"
