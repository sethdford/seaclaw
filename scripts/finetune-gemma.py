#!/usr/bin/env python3
"""
Fine-tune Gemma 4 31B with LoRA using MLX.

Wraps mlx_lm.lora with defaults tuned for persona fine-tuning on Apple Silicon.
Stops the MLX server before training (they compete for memory), restarts after.

Usage:
  python3 scripts/finetune-gemma.py --data ~/.human/training-data/finetune
  python3 scripts/finetune-gemma.py --data ~/.human/training-data/finetune --iters 200 --rank 16
  python3 scripts/finetune-gemma.py --resume  # resume from last adapter checkpoint

Prerequisites:
  pip install mlx-lm (already installed if mlx-server.py works)
"""
from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

DEFAULT_MODEL = "mlx-community/gemma-4-31b-it-4bit"
DEFAULT_ADAPTER_PATH = str(Path.home() / ".human" / "training-data" / "adapters" / "seth-lora")
DEFAULT_DATA = str(Path.home() / ".human" / "training-data" / "finetune")
MLX_SERVER_PORT = 8741


def find_mlx_server_pid() -> int | None:
    """Find the PID of the running MLX server."""
    try:
        result = subprocess.run(
            ["lsof", "-ti", f":{MLX_SERVER_PORT}"],
            capture_output=True, text=True, timeout=5,
        )
        if result.returncode == 0 and result.stdout.strip():
            pids = result.stdout.strip().split("\n")
            return int(pids[0])
    except Exception:
        pass
    return None


def stop_mlx_server() -> int | None:
    """Stop the MLX server to free GPU memory for training."""
    pid = find_mlx_server_pid()
    if pid:
        print(f"  Stopping MLX server (PID {pid}) to free memory for training...")
        os.kill(pid, signal.SIGTERM)
        time.sleep(3)
        try:
            os.kill(pid, 0)
            os.kill(pid, signal.SIGKILL)
            time.sleep(1)
        except ProcessLookupError:
            pass
        print("  MLX server stopped.")
        return pid
    print("  No MLX server running (port 8741 free).")
    return None


def start_mlx_server(model: str, adapter_path: str | None = None):
    """Restart the MLX server, optionally with a LoRA adapter."""
    cmd = [
        sys.executable,
        str(Path(__file__).parent / "mlx-server.py"),
        "--model", model,
        "--port", str(MLX_SERVER_PORT),
    ]
    if adapter_path and Path(adapter_path).exists():
        cmd.extend(["--adapter-path", adapter_path])

    print(f"\n  Restarting MLX server...")
    print(f"    Model: {model}")
    if adapter_path:
        print(f"    Adapter: {adapter_path}")

    proc = subprocess.Popen(
        cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        start_new_session=True,
    )
    time.sleep(5)

    if proc.poll() is None:
        print(f"  MLX server started (PID {proc.pid}) on port {MLX_SERVER_PORT}")
    else:
        print(f"  WARNING: MLX server exited with code {proc.returncode}")


def run_finetune(args):
    print(f"\n{'='*60}")
    print(f"  Gemma 4 31B LoRA Fine-Tune")
    print(f"{'='*60}")
    print(f"  Model:       {args.model}")
    print(f"  Data:        {args.data}")
    print(f"  Adapter:     {args.adapter_path}")
    print(f"  Iterations:  {args.iters}")
    print(f"  Batch size:  {args.batch_size}")
    print(f"  LoRA rank:   {args.rank}")
    print(f"  LoRA layers: {args.num_layers}")
    print(f"  LR:          {args.learning_rate}")
    print(f"  Mask prompt: {args.mask_prompt}")
    print(f"{'='*60}\n")

    data_dir = Path(args.data)
    if not (data_dir / "train.jsonl").exists():
        print(f"ERROR: {data_dir / 'train.jsonl'} not found.")
        print(f"Run: python3 scripts/prepare-finetune.py --persona seth --include-chatdb --output {data_dir}")
        sys.exit(1)

    train_count = sum(1 for _ in open(data_dir / "train.jsonl"))
    valid_count = sum(1 for _ in open(data_dir / "valid.jsonl")) if (data_dir / "valid.jsonl").exists() else 0
    print(f"  Training examples: {train_count}")
    print(f"  Validation examples: {valid_count}\n")

    was_running = stop_mlx_server()

    adapter_dir = Path(args.adapter_path)
    adapter_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        sys.executable, "-m", "mlx_lm", "lora",
        "--model", args.model,
        "--train",
        "--data", str(data_dir),
        "--fine-tune-type", "lora",
        "--adapter-path", str(adapter_dir),
        "--iters", str(args.iters),
        "--batch-size", str(args.batch_size),
        "--learning-rate", str(args.learning_rate),
        "--num-layers", str(args.num_layers),
        "--steps-per-report", str(args.steps_per_report),
        "--steps-per-eval", str(args.steps_per_eval),
        "--save-every", str(args.save_every),
        "--max-seq-length", str(args.max_seq_length),
        "--grad-checkpoint",
        "--optimizer", "adamw",
    ]

    if args.mask_prompt:
        cmd.append("--mask-prompt")

    if args.resume and (adapter_dir / "adapters.safetensors").exists():
        cmd.extend(["--resume-adapter-file", str(adapter_dir / "adapters.safetensors")])
        print(f"  Resuming from {adapter_dir / 'adapters.safetensors'}\n")

    lora_config = {
        "model": args.model,
        "adapter_path": str(adapter_dir),
        "rank": args.rank,
        "iters": args.iters,
        "batch_size": args.batch_size,
        "learning_rate": args.learning_rate,
        "data": str(data_dir),
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
    }
    with open(adapter_dir / "train_config.json", "w") as f:
        json.dump(lora_config, f, indent=2)

    print(f"  Running: {' '.join(cmd[:8])}...")
    print(f"  (This will take 20-60 minutes depending on data size)\n")

    t0 = time.time()
    result = subprocess.run(cmd, cwd=str(Path(__file__).parent.parent))
    elapsed = time.time() - t0

    print(f"\n{'='*60}")
    if result.returncode == 0:
        print(f"  Fine-tuning complete! ({elapsed/60:.1f} minutes)")
        print(f"  Adapter saved to: {adapter_dir}")

        if (adapter_dir / "adapters.safetensors").exists():
            size_mb = (adapter_dir / "adapters.safetensors").stat().st_size / (1024 * 1024)
            print(f"  Adapter size: {size_mb:.1f} MB")

        if not args.no_restart_server:
            start_mlx_server(args.model, str(adapter_dir))

        print(f"\n  Your model is now fine-tuned and serving!")
        print(f"  Test it: curl http://127.0.0.1:{MLX_SERVER_PORT}/v1/chat/completions \\")
        print(f'    -d \'{{"messages":[{{"role":"user","content":"hey whats up"}}]}}\'')
    else:
        print(f"  Fine-tuning FAILED (exit code {result.returncode}, {elapsed:.0f}s)")
        if was_running and not args.no_restart_server:
            start_mlx_server(args.model)
    print(f"{'='*60}\n")

    return result.returncode


def main():
    parser = argparse.ArgumentParser(description="Fine-tune Gemma 4 31B with LoRA on MLX")
    parser.add_argument("--model", default=DEFAULT_MODEL, help=f"Base model (default: {DEFAULT_MODEL})")
    parser.add_argument("--data", default=DEFAULT_DATA, help="Training data directory")
    parser.add_argument("--adapter-path", default=DEFAULT_ADAPTER_PATH, help="Where to save the LoRA adapter")
    parser.add_argument("--iters", type=int, default=200, help="Training iterations (default: 200)")
    parser.add_argument("--batch-size", type=int, default=1, help="Batch size (default: 1, safe for 31B)")
    parser.add_argument("--learning-rate", type=float, default=1e-6, help="Learning rate (default: 1e-6, safe for 31B 4-bit)")
    parser.add_argument("--rank", type=int, default=8, help="LoRA rank (default: 8)")
    parser.add_argument("--num-layers", type=int, default=8, help="Number of layers to apply LoRA (default: 8)")
    parser.add_argument("--max-seq-length", type=int, default=1024, help="Max sequence length (default: 1024)")
    parser.add_argument("--steps-per-report", type=int, default=5, help="Report every N steps")
    parser.add_argument("--steps-per-eval", type=int, default=20, help="Evaluate every N steps")
    parser.add_argument("--save-every", type=int, default=20, help="Save checkpoint every N steps")
    parser.add_argument("--mask-prompt", action="store_true", default=True,
                        help="Mask prompt tokens (only train on responses, default: true)")
    parser.add_argument("--no-mask-prompt", action="store_false", dest="mask_prompt")
    parser.add_argument("--resume", action="store_true", help="Resume from existing adapter")
    parser.add_argument("--no-restart-server", action="store_true", help="Don't restart MLX server after training")
    args = parser.parse_args()

    sys.exit(run_finetune(args))


if __name__ == "__main__":
    main()
