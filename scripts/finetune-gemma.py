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


def version_adapter(adapter_dir: Path, config: dict) -> Path:
    """Save a versioned copy of the adapter with metadata. Returns the versioned path."""
    base = adapter_dir.parent
    version = 1
    while (base / f"seth-lora-v{version}").exists():
        version += 1
    versioned = base / f"seth-lora-v{version}"
    shutil.copytree(adapter_dir, versioned)

    config["version"] = version
    config["versioned_path"] = str(versioned)
    with open(versioned / "train_config.json", "w") as f:
        json.dump(config, f, indent=2)

    current_link = base / "seth-lora-current"
    if current_link.is_symlink():
        current_link.unlink()
    elif current_link.exists():
        shutil.rmtree(current_link)
    current_link.symlink_to(versioned.name)

    print(f"  Versioned adapter: {versioned}")
    print(f"  Current symlink: {current_link} -> {versioned.name}")
    return versioned


def run_sft(args, data_dir: Path, adapter_dir: Path) -> int:
    """Run SFT LoRA fine-tuning. Returns exit code."""
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

    print(f"  Running SFT: {' '.join(cmd[:8])}...")
    print(f"  (This will take 20-60 minutes depending on data size)\n")

    t0 = time.time()
    result = subprocess.run(cmd, cwd=str(Path(__file__).parent.parent))
    elapsed = time.time() - t0

    if result.returncode == 0:
        print(f"  SFT complete! ({elapsed/60:.1f} minutes)")
        if (adapter_dir / "adapters.safetensors").exists():
            size_mb = (adapter_dir / "adapters.safetensors").stat().st_size / (1024 * 1024)
            print(f"  Adapter size: {size_mb:.1f} MB")
    else:
        print(f"  SFT FAILED (exit code {result.returncode}, {elapsed:.0f}s)")

    return result.returncode


def find_dpo_data(data_dir: Path) -> Path | None:
    """Look for DPO pairs in standard locations."""
    candidates = [
        data_dir / "dpo" / "pairs.jsonl",
        data_dir / "dpo_pairs.jsonl",
        data_dir.parent / "dpo" / "pairs.jsonl",
    ]
    import glob
    for pattern in sorted(Path(__file__).parent.parent.glob("convo-training*/dpo_pairs.db")):
        candidates.append(pattern)

    for c in candidates:
        if c.exists():
            return c
    return None


def prepare_dpo_from_db(db_path: Path, output_dir: Path) -> Path | None:
    """Convert DPO pairs from SQLite DB to JSONL format for mlx_lm DPO."""
    import sqlite3
    try:
        conn = sqlite3.connect(str(db_path))
        rows = conn.execute("SELECT prompt, chosen, rejected, margin FROM dpo_pairs").fetchall()
        conn.close()
    except Exception as e:
        print(f"  Could not read DPO DB {db_path}: {e}")
        return None

    if not rows:
        return None

    output_dir.mkdir(parents=True, exist_ok=True)
    train_path = output_dir / "train.jsonl"
    valid_path = output_dir / "valid.jsonl"

    import random
    random.shuffle(rows)
    split = int(len(rows) * 0.9)

    for path, data in [(train_path, rows[:split]), (valid_path, rows[split:])]:
        with open(path, "w") as f:
            for prompt, chosen, rejected, margin in data:
                if not prompt or not chosen or not rejected:
                    continue
                entry = {
                    "prompt": prompt,
                    "chosen": chosen,
                    "rejected": rejected,
                }
                f.write(json.dumps(entry) + "\n")

    print(f"  DPO data: {len(rows)} pairs ({split} train, {len(rows)-split} valid)")
    return output_dir


def run_dpo(args, adapter_dir: Path) -> int:
    """Run DPO training pass on top of SFT adapter."""
    data_dir = Path(args.data)

    dpo_source = find_dpo_data(data_dir)
    if dpo_source is None:
        print("  No DPO data found, skipping DPO pass.")
        return 0

    dpo_data_dir = data_dir / "dpo_prepared"
    if str(dpo_source).endswith(".db"):
        result = prepare_dpo_from_db(dpo_source, dpo_data_dir)
        if result is None:
            print("  DPO DB was empty, skipping DPO pass.")
            return 0
    elif str(dpo_source).endswith(".jsonl"):
        dpo_data_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy(dpo_source, dpo_data_dir / "train.jsonl")
    else:
        print(f"  Unknown DPO format: {dpo_source}")
        return 0

    dpo_iters = max(50, args.iters // 4)
    dpo_lr = args.learning_rate * 0.1

    cmd = [
        sys.executable, "-m", "mlx_lm", "lora",
        "--model", args.model,
        "--train",
        "--data", str(dpo_data_dir),
        "--fine-tune-type", "dpo",
        "--adapter-path", str(adapter_dir),
        "--resume-adapter-file", str(adapter_dir / "adapters.safetensors"),
        "--iters", str(dpo_iters),
        "--batch-size", "1",
        "--learning-rate", str(dpo_lr),
        "--num-layers", str(args.num_layers),
        "--max-seq-length", str(args.max_seq_length),
        "--grad-checkpoint",
    ]

    print(f"\n  Running DPO: {dpo_iters} iters, LR={dpo_lr}")
    print(f"  (Sharpening human/AI boundary with preference pairs)\n")

    t0 = time.time()
    result = subprocess.run(cmd, cwd=str(Path(__file__).parent.parent))
    elapsed = time.time() - t0

    if result.returncode == 0:
        print(f"  DPO complete! ({elapsed/60:.1f} minutes)")
    else:
        print(f"  DPO failed (exit code {result.returncode}, {elapsed:.0f}s)")
        print(f"  SFT adapter is still valid — DPO failure is non-fatal.")

    return 0


def run_finetune(args):
    print(f"\n{'='*60}")
    print(f"  Gemma 4 31B LoRA Fine-Tune Pipeline")
    print(f"{'='*60}")
    print(f"  Model:       {args.model}")
    print(f"  Data:        {args.data}")
    print(f"  Adapter:     {args.adapter_path}")
    print(f"  Iterations:  {args.iters}")
    print(f"  Batch size:  {args.batch_size}")
    print(f"  LoRA rank:   {args.rank}")
    print(f"  LoRA layers: {args.num_layers}")
    print(f"  LR:          {args.learning_rate}")
    print(f"  Seq length:  {args.max_seq_length}")
    print(f"  Mask prompt: {args.mask_prompt}")
    print(f"  DPO pass:    {'yes' if args.dpo else 'no'}")
    print(f"  Versioning:  {'yes' if not args.no_version else 'no'}")
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

    # Phase 1: SFT
    print(f"\n  === Phase 1: Supervised Fine-Tuning ===\n")
    rc = run_sft(args, data_dir, adapter_dir)
    if rc != 0:
        if was_running and not args.no_restart_server:
            start_mlx_server(args.model)
        return rc

    # Phase 2: DPO (optional)
    if args.dpo and not args.sft_only:
        print(f"\n  === Phase 2: Direct Preference Optimization ===\n")
        run_dpo(args, adapter_dir)

    # Phase 3: Version the adapter
    lora_config = {
        "model": args.model,
        "adapter_path": str(adapter_dir),
        "rank": args.rank,
        "iters": args.iters,
        "batch_size": args.batch_size,
        "learning_rate": args.learning_rate,
        "max_seq_length": args.max_seq_length,
        "dpo": args.dpo,
        "data": str(data_dir),
        "train_examples": train_count,
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
    }

    if not args.no_version:
        print(f"\n  === Phase 3: Adapter Versioning ===\n")
        version_adapter(adapter_dir, lora_config)
    else:
        with open(adapter_dir / "train_config.json", "w") as f:
            json.dump(lora_config, f, indent=2)

    # Phase 4: Restart server
    print(f"\n{'='*60}")
    if (adapter_dir / "adapters.safetensors").exists():
        print(f"  Pipeline complete!")
        print(f"  Adapter: {adapter_dir}")
        if (adapter_dir / "adapters.safetensors").exists():
            size_mb = (adapter_dir / "adapters.safetensors").stat().st_size / (1024 * 1024)
            print(f"  Size: {size_mb:.1f} MB")

        if not args.no_restart_server:
            start_mlx_server(args.model, str(adapter_dir))

        print(f"\n  Test it: curl http://127.0.0.1:{MLX_SERVER_PORT}/v1/chat/completions \\")
        print(f'    -d \'{{"messages":[{{"role":"user","content":"hey whats up"}}]}}\'')
    else:
        print(f"  Fine-tuning FAILED — no adapter produced")
        if was_running and not args.no_restart_server:
            start_mlx_server(args.model)
    print(f"{'='*60}\n")

    return 0


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
    parser.add_argument("--max-seq-length", type=int, default=2048, help="Max sequence length (default: 2048)")
    parser.add_argument("--steps-per-report", type=int, default=5, help="Report every N steps")
    parser.add_argument("--steps-per-eval", type=int, default=20, help="Evaluate every N steps")
    parser.add_argument("--save-every", type=int, default=20, help="Save checkpoint every N steps")
    parser.add_argument("--mask-prompt", action="store_true", default=True,
                        help="Mask prompt tokens (only train on responses, default: true)")
    parser.add_argument("--no-mask-prompt", action="store_false", dest="mask_prompt")
    parser.add_argument("--dpo", action="store_true", default=True,
                        help="Run DPO pass after SFT if DPO data exists (default: true)")
    parser.add_argument("--sft-only", action="store_true", help="Skip DPO even if data exists")
    parser.add_argument("--no-version", action="store_true", help="Don't version the adapter")
    parser.add_argument("--resume", action="store_true", help="Resume from existing adapter")
    parser.add_argument("--no-restart-server", action="store_true", help="Don't restart MLX server after training")
    args = parser.parse_args()

    sys.exit(run_finetune(args))


if __name__ == "__main__":
    main()
