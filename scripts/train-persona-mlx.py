#!/usr/bin/env python3
"""Fine-tune Gemma 4 on persona conversation data using mlx-tune.

Usage:
    python3 scripts/train-persona-mlx.py                          # SFT training
    python3 scripts/train-persona-mlx.py --model mlx-community/gemma-4-e4b-it-4bit  # smaller model
    python3 scripts/train-persona-mlx.py --serve                  # serve after training
    python3 scripts/train-persona-mlx.py --serve-only             # serve existing adapter

Requires: pip install mlx-tune
"""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MODEL = "mlx-community/gemma-4-26b-a4b-it-4bit"
DEFAULT_DATA_DIR = REPO_ROOT / "training-data" / "persona"
DEFAULT_ADAPTER_DIR = REPO_ROOT / "adapters" / "persona-v1"


def train_sft(model_name: str, data_dir: Path, adapter_dir: Path,
              max_steps: int = 200, lr: float = 2e-4, lora_rank: int = 16,
              max_seq_length: int = 2048, batch_size: int = 1):
    """Run SFT training with mlx-tune."""
    adapter_dir.mkdir(parents=True, exist_ok=True)

    train_file = data_dir / "train.jsonl"
    if not train_file.exists():
        print(f"Error: {train_file} not found. Run finetune-persona.py --collect first.")
        sys.exit(1)

    n_train = sum(1 for _ in open(train_file))
    n_epochs = max(3, max_steps // max(1, n_train))

    script = f'''
import json, os
from pathlib import Path
from mlx_tune import FastLanguageModel
from mlx_tune import apply_chat_template
from datasets import Dataset

model, tokenizer = FastLanguageModel.from_pretrained(
    model_name="{model_name}",
    max_seq_length={max_seq_length},
    load_in_4bit=True,
)

model = FastLanguageModel.get_peft_model(
    model,
    r={lora_rank},
    target_modules=["q_proj", "k_proj", "v_proj", "o_proj",
                     "gate_proj", "up_proj", "down_proj"],
    lora_alpha={lora_rank * 2},
    lora_dropout=0.0,
    use_gradient_checkpointing=True,
)

# Load training data
train_data = []
with open("{train_file}") as f:
    for line in f:
        train_data.append(json.loads(line))

dataset = Dataset.from_list(train_data)
dataset = apply_chat_template(dataset, tokenizer)

from mlx_tune import SFTTrainer, TrainingArguments

trainer = SFTTrainer(
    model=model,
    tokenizer=tokenizer,
    train_dataset=dataset,
    args=TrainingArguments(
        output_dir="{adapter_dir}",
        max_steps={max_steps},
        per_device_train_batch_size={batch_size},
        learning_rate={lr},
        logging_steps=10,
        save_steps=50,
        warmup_steps=10,
        gradient_accumulation_steps=4,
        fp16=False,
        bf16=True,
        seed=42,
    ),
    max_seq_length={max_seq_length},
)

print(f"Training {{len(train_data)}} examples for {{max_steps}} steps...")
trainer.train()

print("Saving adapter...")
model.save_pretrained("{adapter_dir}")
tokenizer.save_pretrained("{adapter_dir}")
print(f"Adapter saved to {adapter_dir}")
'''

    script_path = adapter_dir / "_train.py"
    script_path.write_text(script)
    print(f"\n== SFT Training ==")
    print(f"  Model: {model_name}")
    print(f"  Data: {n_train} examples from {train_file}")
    print(f"  Steps: {max_steps}")
    print(f"  LoRA rank: {lora_rank}")
    print(f"  LR: {lr}")
    print(f"  Adapter: {adapter_dir}\n")

    result = subprocess.run(
        [sys.executable, str(script_path)],
        cwd=str(REPO_ROOT),
    )
    return result.returncode


def train_with_mlx_lm(model_name: str, data_dir: Path, adapter_dir: Path,
                       iters: int = 200, lr: float = 1e-5, lora_rank: int = 16):
    """Fallback: use mlx_lm.lora directly."""
    adapter_dir.mkdir(parents=True, exist_ok=True)

    train_file = data_dir / "train.jsonl"
    n_train = sum(1 for _ in open(train_file))

    cmd = [
        sys.executable, "-m", "mlx_lm.lora",
        "--model", model_name,
        "--train",
        "--data", str(data_dir),
        "--adapter-path", str(adapter_dir),
        "--iters", str(iters),
        "--batch-size", "1",
        "--learning-rate", str(lr),
        "--mask-prompt",
        "--grad-checkpoint",
        "--steps-per-report", "10",
        "--steps-per-eval", "50",
        "--save-every", "50",
    ]

    print(f"\n== SFT Training (mlx-lm) ==")
    print(f"  Model: {model_name}")
    print(f"  Data: {n_train} examples from {train_file}")
    print(f"  Iterations: {iters}")
    print(f"  LoRA rank: {lora_rank}")
    print(f"  LR: {lr}")
    print(f"  Adapter: {adapter_dir}")
    print(f"  Command: {' '.join(cmd)}\n")

    result = subprocess.run(cmd, cwd=str(REPO_ROOT))
    return result.returncode


def serve(model_name: str, adapter_dir: Path, port: int = 8800):
    """Serve the fine-tuned model via mlx_lm.server."""
    cmd = [
        sys.executable, "-m", "mlx_lm.server",
        "--model", model_name,
        "--adapter-path", str(adapter_dir),
        "--port", str(port),
    ]
    print(f"\n== Serving Fine-tuned Model ==")
    print(f"  Base: {model_name}")
    print(f"  Adapter: {adapter_dir}")
    print(f"  URL: http://localhost:{port}")
    print(f"\n  To use with h-uman:")
    print(f"    HUMAN_PROVIDER=mlx_local HUMAN_MODEL=mlx-community/gemma-4-26b-a4b-it-4bit ./build/human agent -m 'hey'")
    print()
    subprocess.run(cmd, cwd=str(REPO_ROOT))


def main():
    parser = argparse.ArgumentParser(description="Fine-tune Gemma 4 on persona data with MLX")
    parser.add_argument("--model", default=DEFAULT_MODEL, help="Base model")
    parser.add_argument("--data", type=Path, default=DEFAULT_DATA_DIR, help="Training data dir")
    parser.add_argument("--adapter-dir", type=Path, default=DEFAULT_ADAPTER_DIR, help="Adapter output")
    parser.add_argument("--steps", type=int, default=200, help="Training steps")
    parser.add_argument("--lr", type=float, default=1e-5, help="Learning rate")
    parser.add_argument("--lora-rank", type=int, default=16, help="LoRA rank")
    parser.add_argument("--use-mlx-lm", action="store_true", help="Use mlx_lm.lora instead of mlx-tune")
    parser.add_argument("--serve", action="store_true", help="Serve model after training")
    parser.add_argument("--serve-only", action="store_true", help="Only serve (no training)")
    parser.add_argument("--port", type=int, default=8800, help="Server port")

    args = parser.parse_args()

    if args.serve_only:
        serve(args.model, args.adapter_dir, args.port)
        return

    if args.use_mlx_lm:
        rc = train_with_mlx_lm(args.model, args.data, args.adapter_dir,
                                iters=args.steps, lr=args.lr, lora_rank=args.lora_rank)
    else:
        rc = train_sft(args.model, args.data, args.adapter_dir,
                       max_steps=args.steps, lr=args.lr, lora_rank=args.lora_rank)

    if rc != 0:
        print(f"\nmlx-tune failed (exit {rc}), falling back to mlx_lm.lora...")
        rc = train_with_mlx_lm(args.model, args.data, args.adapter_dir,
                                iters=args.steps, lr=args.lr, lora_rank=args.lora_rank)

    if rc == 0 and args.serve:
        serve(args.model, args.adapter_dir, args.port)
    elif rc != 0:
        print(f"\nTraining failed with exit code {rc}", file=sys.stderr)
        sys.exit(rc)


if __name__ == "__main__":
    main()
