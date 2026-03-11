#!/usr/bin/env python3
"""
Prepare iMessage training data for MLX LoRA fine-tuning.
Converts training_pairs.jsonl to MLX chat format with train/valid/test splits.
"""

import json
import os
import random

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "imessage")
OUT_DIR = os.path.join(DATA_DIR, "mlx_training")

SYSTEM_PROMPT = (
    "You are Seth. You text like a real person — short messages, slang, emojis, "
    "typos, strong opinions. You never write paragraphs in texts. "
    "You use words like 'lyk', 'tbh', 'HRU', 'roady'. "
    "You're affectionate, direct, sometimes anxious, always genuine."
)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    pairs_path = os.path.join(DATA_DIR, "training_pairs.jsonl")
    with open(pairs_path) as f:
        pairs = [json.loads(line) for line in f]

    print(f"Loaded {len(pairs)} training pairs")

    examples = []
    for pair in pairs:
        msgs = pair["messages"]
        if len(msgs) < 2:
            continue

        merged = []
        for m in msgs:
            if merged and merged[-1]["role"] == m["role"]:
                merged[-1]["content"] += " " + m["content"]
            else:
                merged.append({"role": m["role"], "content": m["content"]})

        if len(merged) < 2 or merged[-1]["role"] != "assistant":
            continue
        if merged[0]["role"] != "user":
            merged = merged[1:]
        if len(merged) < 2:
            continue

        chat_msgs = [{"role": "system", "content": SYSTEM_PROMPT}]
        for m in merged:
            chat_msgs.append(m)
        examples.append({"messages": chat_msgs})

    random.seed(42)
    random.shuffle(examples)

    n = len(examples)
    train_end = int(n * 0.8)
    valid_end = int(n * 0.9)

    train = examples[:train_end]
    valid = examples[train_end:valid_end]
    test = examples[valid_end:]

    for split, data in [("train", train), ("valid", valid), ("test", test)]:
        path = os.path.join(OUT_DIR, f"{split}.jsonl")
        with open(path, "w") as f:
            for ex in data:
                f.write(json.dumps(ex) + "\n")
        print(f"  {split}: {len(data)} examples -> {path}")

    print(f"\nSample training example:")
    if train:
        print(json.dumps(train[0], indent=2)[:500])


if __name__ == "__main__":
    main()
