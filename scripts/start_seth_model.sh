#!/bin/bash
# Start the fine-tuned Seth model server (MLX, OpenAI-compatible API)
# Usage: ./scripts/start_seth_model.sh [port]

PORT="${1:-8800}"
MODEL_PATH="$(dirname "$0")/../data/imessage/seth-gemma3-4b-fused"

if ! command -v python3 &>/dev/null; then
    echo "Error: python3 not found"
    exit 1
fi

python3 -c "import mlx_lm" 2>/dev/null || {
    echo "Error: mlx_lm not installed. Run: pip3 install mlx-lm"
    exit 1
}

if [ ! -d "$MODEL_PATH" ]; then
    echo "Error: Model not found at $MODEL_PATH"
    echo "Run the fine-tuning pipeline first: python3 scripts/extract_imessage_pairs.py && ..."
    exit 1
fi

echo "Starting Seth model server on port $PORT..."
echo "Model: $MODEL_PATH"
echo "API: http://localhost:$PORT/v1"
echo ""
exec python3 -m mlx_lm.server --model "$MODEL_PATH" --port "$PORT"
