#!/usr/bin/env python3
"""
Prepare fine-tuning data by merging iMessage training pairs with DPO preferences.

Reads:
- data/imessage/training_pairs.jsonl — multi-turn conversation windows
- data/imessage/dpo_preferences.jsonl — preference pairs from runtime
- data/imessage/ground_truth.jsonl — real Seth responses

Produces:
- data/imessage/finetune_merged.jsonl — combined training data for LoRA
- data/imessage/finetune_stats.json — statistics about the training data
"""

import json
import os
import sys
from datetime import datetime

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "imessage")


def load_jsonl(path):
    if not os.path.exists(path):
        return []
    entries = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line:
                entries.append(json.loads(line))
    return entries


def convert_dpo_to_training(dpo_pair):
    """Convert a DPO preference pair to a training example using the chosen response."""
    return {
        "messages": [
            {"role": "user", "content": dpo_pair.get("prompt", "")},
            {"role": "assistant", "content": dpo_pair.get("chosen", "")},
        ],
        "metadata": {
            "source": "dpo_chosen",
            "margin": dpo_pair.get("margin", 0),
            "original_source": dpo_pair.get("source", "unknown"),
        },
    }


def convert_ground_truth_to_training(gt_pair):
    """Convert a ground truth pair to a training example."""
    return {
        "messages": [
            {"role": "user", "content": gt_pair["incoming"]},
            {"role": "assistant", "content": gt_pair["seth_reply"]},
        ],
        "metadata": {
            "source": "ground_truth",
            "delay_seconds": gt_pair.get("delay_seconds"),
            "chat_id": gt_pair.get("chat_id"),
        },
    }


def deduplicate(entries):
    """Remove exact duplicate assistant responses."""
    seen = set()
    unique = []
    for entry in entries:
        msgs = entry.get("messages", [])
        if not msgs:
            continue
        last_msg = msgs[-1].get("content", "")
        if last_msg in seen:
            continue
        seen.add(last_msg)
        unique.append(entry)
    return unique


def compute_stats(entries):
    sources = {}
    lengths = []
    for entry in entries:
        src = entry.get("metadata", {}).get("source", "training_pairs")
        sources[src] = sources.get(src, 0) + 1
        msgs = entry.get("messages", [])
        for m in msgs:
            if m.get("role") == "assistant":
                lengths.append(len(m.get("content", "")))

    avg_len = sum(lengths) / len(lengths) if lengths else 0
    median_len = sorted(lengths)[len(lengths) // 2] if lengths else 0

    return {
        "total_examples": len(entries),
        "by_source": sources,
        "avg_assistant_length": round(avg_len, 1),
        "median_assistant_length": median_len,
        "generated_at": datetime.now().isoformat(),
    }


def main():
    training_path = os.path.join(DATA_DIR, "training_pairs.jsonl")
    dpo_path = os.path.join(DATA_DIR, "dpo_preferences.jsonl")
    gt_path = os.path.join(DATA_DIR, "ground_truth.jsonl")
    out_path = os.path.join(DATA_DIR, "finetune_merged.jsonl")
    stats_path = os.path.join(DATA_DIR, "finetune_stats.json")

    print("Loading data sources...")
    training = load_jsonl(training_path)
    dpo = load_jsonl(dpo_path)
    gt = load_jsonl(gt_path)

    print(f"  Training pairs: {len(training)}")
    print(f"  DPO preferences: {len(dpo)}")
    print(f"  Ground truth: {len(gt)}")

    for t in training:
        t.setdefault("metadata", {})["source"] = "training_pairs"

    all_entries = list(training)

    for pair in dpo:
        all_entries.append(convert_dpo_to_training(pair))

    for pair in gt:
        all_entries.append(convert_ground_truth_to_training(pair))

    print(f"\n  Total before dedup: {len(all_entries)}")
    all_entries = deduplicate(all_entries)
    print(f"  Total after dedup: {len(all_entries)}")

    os.makedirs(DATA_DIR, exist_ok=True)
    with open(out_path, "w") as f:
        for entry in all_entries:
            f.write(json.dumps(entry) + "\n")
    print(f"\n  Merged data: {len(all_entries)} -> {out_path}")

    stats = compute_stats(all_entries)
    with open(stats_path, "w") as f:
        json.dump(stats, f, indent=2)
    print(f"  Stats: {stats_path}")

    print(f"\n--- Stats ---")
    for k, v in stats.items():
        print(f"  {k}: {v}")


if __name__ == "__main__":
    main()
