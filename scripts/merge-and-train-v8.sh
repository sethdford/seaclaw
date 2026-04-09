#!/usr/bin/env bash
set -euo pipefail

# merge-and-train-v8.sh — Merge all training data and train with DoRA
# Usage: bash scripts/merge-and-train-v8.sh

cd "$(dirname "$0")/.."
TD="training-data"
MODEL="mlx-community/gemma-4-26b-a4b-it-4bit"
ADAPTER_DIR="adapters/persona-v8"

echo "=== Step 1: Merge base + synthetic training data ==="
python3 - << 'PYEOF'
import json, random

base = []
with open("training-data/base-v8.jsonl") as f:
    for line in f:
        if line.strip():
            base.append(json.loads(line))
print(f"  Base examples: {len(base)}")

synthetic = []
if __import__("os").path.exists("training-data/synthetic-v8-gemini.jsonl"):
    with open("training-data/synthetic-v8-gemini.jsonl") as f:
        for line in f:
            if line.strip():
                synthetic.append(json.loads(line))
print(f"  Synthetic examples: {len(synthetic)}")

# Update synthetic system prompts to match base
if base:
    sys_prompt = base[0].get("messages", [{}])[0].get("content", "")
    for ex in synthetic:
        msgs = ex.get("messages", [])
        if msgs and msgs[0].get("role") == "system":
            msgs[0]["content"] = sys_prompt

all_examples = base + synthetic
random.shuffle(all_examples)

# 90/10 train/valid split
split = max(1, int(len(all_examples) * 0.1))
valid = all_examples[:split]
train = all_examples[split:]

with open("training-data/train.jsonl", "w") as f:
    for ex in train:
        f.write(json.dumps(ex) + "\n")
with open("training-data/valid.jsonl", "w") as f:
    for ex in valid:
        f.write(json.dumps(ex) + "\n")

print(f"  Total: {len(all_examples)} (train: {len(train)}, valid: {len(valid)})")
PYEOF

echo ""
echo "=== Step 2: Check disk space ==="
df -h . | tail -1
echo ""

echo "=== Step 3: Clean old adapter checkpoints ==="
for d in adapters/persona-v{1,2,3,4,5,6}; do
    if [ -d "$d" ]; then
        echo "  Removing $d"
        rm -rf "$d"
    fi
done
echo ""

echo "=== Step 4: Train with LoRA (aggressive settings for deep override) ==="
echo "  Model: $MODEL"
echo "  Adapter: $ADAPTER_DIR"
echo "  Layers: 16 (up from 12)"
echo "  Iterations: 1000 (up from 600)"
echo "  Learning rate: 2e-6 (lower for stability with more data)"
echo "  Rank: 8"
echo ""

python3 -m mlx_lm.lora \
    --model "$MODEL" \
    --train \
    --data "$TD" \
    --fine-tune-type lora \
    --num-layers 16 \
    --batch-size 1 \
    --iters 1000 \
    --learning-rate 2e-6 \
    --adapter-path "$ADAPTER_DIR" \
    --save-every 200 \
    --val-batches 5

echo ""
echo "=== Step 5: Install adapter ==="
mkdir -p ~/.human/adapters/persona
cp "$ADAPTER_DIR/adapters.safetensors" ~/.human/adapters/persona/
cp "$ADAPTER_DIR/adapter_config.json" ~/.human/adapters/persona/
echo "  Installed to ~/.human/adapters/persona/"

echo ""
echo "=== Step 6: Restart MLX server ==="
if [ -f scripts/human-serve.sh ]; then
    bash scripts/human-serve.sh restart
elif [ -f ~/.human/bin/human-serve.sh ]; then
    bash ~/.human/bin/human-serve.sh restart
else
    echo "  human-serve.sh not found — restart server manually"
fi

echo ""
echo "=== Done! v8 adapter trained and installed ==="
