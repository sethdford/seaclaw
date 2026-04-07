#!/usr/bin/env python3
"""Fine-tune Gemma 4 on persona conversation data using mlx-lm LoRA.

Collects DPO pairs and persona corrections from convo-trainer runs,
converts them to mlx-lm chat JSONL format, runs LoRA fine-tuning,
and optionally serves the fine-tuned model.

Usage:
    # Export training data + fine-tune
    python3 scripts/finetune-persona.py --collect /tmp/convo-gemma4-* /tmp/convo-redteam-*

    # Fine-tune with existing data
    python3 scripts/finetune-persona.py --data training-data/persona

    # Serve the fine-tuned model
    python3 scripts/finetune-persona.py --serve

    # Full pipeline: collect + augment + fine-tune
    python3 scripts/finetune-persona.py --collect /tmp/convo-* --augment --train

Requires: pip install "mlx-lm[train]"
"""
import argparse
import glob
import json
import os
import random
import shutil
import sqlite3
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BASE_MODEL = "mlx-community/gemma-4-12b-it-4bit"
DEFAULT_OUTPUT_DIR = REPO_ROOT / "training-data" / "persona"
DEFAULT_ADAPTER_DIR = REPO_ROOT / "adapters" / "persona-v1"
DEFAULT_PERSONA = REPO_ROOT / "data" / "personas" / "default.json"


def load_persona(path: Path) -> dict:
    with open(path) as f:
        return json.load(f)


def build_system_prompt(persona: dict) -> str:
    """Build a compact system prompt from persona for training examples."""
    parts = []
    if persona.get("identity"):
        parts.append(f"You ARE this person: {persona['identity']}")
    if persona.get("biography"):
        parts.append(persona["biography"])
    parts.append(
        "Output ONLY what this person would actually type — nothing else. "
        "No reasoning, no parentheses, no meta-commentary."
    )
    for rule in persona.get("communication_rules", []):
        parts.append(f"- {rule}")
    return "\n".join(parts)


def collect_sft_data(dirs: list[str], persona: dict) -> list[dict]:
    """Collect SFT training examples from convo-trainer output directories."""
    system_prompt = build_system_prompt(persona)
    examples = []

    for pattern in dirs:
        for d in sorted(glob.glob(pattern)):
            d = Path(d)

            # Persona corrections (preferred format: has context + corrected response)
            feedback_file = d / "persona_feedback.jsonl"
            if feedback_file.exists():
                for line in feedback_file.read_text().strip().split("\n"):
                    if not line.strip():
                        continue
                    entry = json.loads(line)
                    ctx = entry.get("context", "")
                    corrected = entry.get("corrected", "")
                    if ctx and corrected:
                        examples.append({
                            "messages": [
                                {"role": "system", "content": system_prompt},
                                {"role": "user", "content": ctx},
                                {"role": "assistant", "content": corrected},
                            ]
                        })

            # DPO pairs — use chosen responses as SFT targets
            dpo_db = d / "dpo_pairs.db"
            if dpo_db.exists():
                try:
                    conn = sqlite3.connect(str(dpo_db))
                    rows = conn.execute(
                        "SELECT prompt, chosen FROM dpo_pairs"
                    ).fetchall()
                    conn.close()
                    for prompt, chosen in rows:
                        if prompt and chosen:
                            examples.append({
                                "messages": [
                                    {"role": "system", "content": system_prompt},
                                    {"role": "user", "content": prompt},
                                    {"role": "assistant", "content": chosen},
                                ]
                            })
                except Exception as e:
                    print(f"  Warning: could not read {dpo_db}: {e}", file=sys.stderr)

    return examples


def collect_dpo_data(dirs: list[str], persona: dict) -> list[dict]:
    """Collect DPO training pairs from convo-trainer output."""
    system_prompt = build_system_prompt(persona)
    pairs = []

    for pattern in dirs:
        for d in sorted(glob.glob(pattern)):
            d = Path(d)
            dpo_db = d / "dpo_pairs.db"
            if not dpo_db.exists():
                continue
            try:
                conn = sqlite3.connect(str(dpo_db))
                rows = conn.execute(
                    "SELECT prompt, chosen, rejected, margin FROM dpo_pairs"
                ).fetchall()
                conn.close()
                for prompt, chosen, rejected, margin in rows:
                    if prompt and chosen and rejected:
                        pairs.append({
                            "prompt": [
                                {"role": "system", "content": system_prompt},
                                {"role": "user", "content": prompt},
                            ],
                            "chosen": [{"role": "assistant", "content": chosen}],
                            "rejected": [{"role": "assistant", "content": rejected}],
                            "margin": margin or 0.5,
                        })
            except Exception as e:
                print(f"  Warning: could not read {dpo_db}: {e}", file=sys.stderr)

    return pairs


def augment_from_persona_examples(persona: dict) -> list[dict]:
    """Create additional training examples from the persona's example bank."""
    system_prompt = build_system_prompt(persona)
    examples = []

    for bank in persona.get("example_banks", []):
        for ex in bank.get("examples", []):
            incoming = ex.get("incoming", "")
            response = ex.get("response", "")
            if incoming and response:
                examples.append({
                    "messages": [
                        {"role": "system", "content": system_prompt},
                        {"role": "user", "content": incoming},
                        {"role": "assistant", "content": response},
                    ]
                })

    return examples


def deduplicate(examples: list[dict]) -> list[dict]:
    """Remove duplicate examples based on user message content."""
    seen = set()
    unique = []
    for ex in examples:
        user_msg = None
        for m in ex.get("messages", []):
            if m["role"] == "user":
                user_msg = m["content"]
                break
        key = user_msg or json.dumps(ex)
        if key not in seen:
            seen.add(key)
            unique.append(ex)
    return unique


def write_splits(examples: list[dict], output_dir: Path, test_ratio: float = 0.15):
    """Write train/test/valid JSONL splits."""
    output_dir.mkdir(parents=True, exist_ok=True)
    random.shuffle(examples)

    n = len(examples)
    n_test = max(1, int(n * test_ratio))
    n_valid = max(1, int(n * test_ratio))
    n_train = n - n_test - n_valid

    if n_train < 1:
        n_train = n
        n_test = 0
        n_valid = 0

    train = examples[:n_train]
    test = examples[n_train : n_train + n_test]
    valid = examples[n_train + n_test :]

    for name, data in [("train", train), ("test", test), ("valid", valid)]:
        path = output_dir / f"{name}.jsonl"
        with open(path, "w") as f:
            for ex in data:
                f.write(json.dumps(ex) + "\n")
        print(f"  {name}: {len(data)} examples -> {path}")


def run_finetune(
    base_model: str,
    data_dir: Path,
    adapter_dir: Path,
    iters: int = 600,
    batch_size: int = 1,
    lora_rank: int = 16,
    learning_rate: float = 1e-5,
    mask_prompt: bool = True,
):
    """Run mlx-lm LoRA fine-tuning."""
    adapter_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        sys.executable, "-m", "mlx_lm.lora",
        "--model", base_model,
        "--train",
        "--data", str(data_dir),
        "--adapter-path", str(adapter_dir),
        "--iters", str(iters),
        "--batch-size", str(batch_size),
        "--lora-rank", str(lora_rank),
        "--learning-rate", str(learning_rate),
    ]
    if mask_prompt:
        cmd.append("--mask-prompt")

    print(f"\n  Running: {' '.join(cmd)}\n")
    result = subprocess.run(cmd, cwd=str(REPO_ROOT))
    return result.returncode


def serve_model(base_model: str, adapter_dir: Path, port: int = 8800):
    """Serve the fine-tuned model via mlx_lm.server."""
    cmd = [
        sys.executable, "-m", "mlx_lm.server",
        "--model", base_model,
        "--adapter-path", str(adapter_dir),
        "--port", str(port),
    ]
    print(f"\n  Serving on http://localhost:{port}")
    print(f"  Command: {' '.join(cmd)}")
    print(f"\n  To use with h-uman:")
    print(f"    export HUMAN_PROVIDER=mlx_local")
    print(f"    export HUMAN_MODEL=gemma-4-persona")
    print(f"    ./build/human agent -m 'hey whats up'\n")
    subprocess.run(cmd, cwd=str(REPO_ROOT))


def main():
    parser = argparse.ArgumentParser(description="Fine-tune Gemma 4 on persona data")
    parser.add_argument(
        "--collect", nargs="+", default=None,
        help="Glob patterns for convo-trainer output dirs to collect data from",
    )
    parser.add_argument(
        "--augment", action="store_true",
        help="Augment training data with persona example bank",
    )
    parser.add_argument(
        "--data", type=Path, default=DEFAULT_OUTPUT_DIR,
        help="Path to training data directory (input or output)",
    )
    parser.add_argument(
        "--train", action="store_true",
        help="Run LoRA fine-tuning after data collection",
    )
    parser.add_argument("--serve", action="store_true", help="Serve the fine-tuned model")
    parser.add_argument("--port", type=int, default=8800, help="Server port (default: 8800)")
    parser.add_argument(
        "--model", default=DEFAULT_BASE_MODEL,
        help=f"Base model for fine-tuning (default: {DEFAULT_BASE_MODEL})",
    )
    parser.add_argument(
        "--adapter-dir", type=Path, default=DEFAULT_ADAPTER_DIR,
        help=f"Adapter output directory (default: {DEFAULT_ADAPTER_DIR})",
    )
    parser.add_argument(
        "--persona", type=Path, default=DEFAULT_PERSONA,
        help=f"Persona JSON file (default: {DEFAULT_PERSONA})",
    )
    parser.add_argument("--iters", type=int, default=600, help="Training iterations")
    parser.add_argument("--batch-size", type=int, default=1, help="Batch size")
    parser.add_argument("--lora-rank", type=int, default=16, help="LoRA rank")
    parser.add_argument("--lr", type=float, default=1e-5, help="Learning rate")

    args = parser.parse_args()
    persona = load_persona(args.persona)

    if args.collect:
        print("\n== Collecting training data ==")
        sft_examples = collect_sft_data(args.collect, persona)
        print(f"  Collected {len(sft_examples)} SFT examples from convo-trainer runs")

        dpo_pairs = collect_dpo_data(args.collect, persona)
        print(f"  Collected {len(dpo_pairs)} DPO pairs")

        if args.augment:
            aug = augment_from_persona_examples(persona)
            print(f"  Augmented with {len(aug)} examples from persona example bank")
            sft_examples.extend(aug)

        sft_examples = deduplicate(sft_examples)
        print(f"  After dedup: {len(sft_examples)} unique examples")

        print(f"\n== Writing SFT splits to {args.data} ==")
        write_splits(sft_examples, args.data)

        if dpo_pairs:
            dpo_dir = args.data / "dpo"
            dpo_dir.mkdir(parents=True, exist_ok=True)
            dpo_path = dpo_dir / "pairs.jsonl"
            with open(dpo_path, "w") as f:
                for pair in dpo_pairs:
                    f.write(json.dumps(pair) + "\n")
            print(f"  DPO pairs: {len(dpo_pairs)} -> {dpo_path}")

    if args.train:
        print("\n== Running LoRA fine-tuning ==")
        train_file = args.data / "train.jsonl"
        if not train_file.exists():
            print(f"  Error: {train_file} not found. Run with --collect first.", file=sys.stderr)
            sys.exit(1)

        n_train = sum(1 for _ in open(train_file))
        print(f"  Training examples: {n_train}")
        print(f"  Base model: {args.model}")
        print(f"  Adapter dir: {args.adapter_dir}")
        print(f"  Iterations: {args.iters}")
        print(f"  LoRA rank: {args.lora_rank}")
        print(f"  Learning rate: {args.lr}")

        rc = run_finetune(
            base_model=args.model,
            data_dir=args.data,
            adapter_dir=args.adapter_dir,
            iters=args.iters,
            batch_size=args.batch_size,
            lora_rank=args.lora_rank,
            learning_rate=args.lr,
        )
        if rc != 0:
            print(f"  Fine-tuning failed with exit code {rc}", file=sys.stderr)
            sys.exit(rc)
        print(f"\n  Adapter saved to: {args.adapter_dir}")

    if args.serve:
        serve_model(args.model, args.adapter_dir, args.port)

    if not args.collect and not args.train and not args.serve:
        parser.print_help()


if __name__ == "__main__":
    main()
