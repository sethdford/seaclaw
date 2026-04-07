#!/usr/bin/env python3
"""
Automated Training Loop — the full cycle that makes Seth more human.

Orchestrates: data extraction -> merge -> prepare -> SFT -> DPO -> eval -> promote/rollback

Each cycle:
  1. Extract fresh data (iMessage, Facebook, Apple Photos)
  2. Merge all sources + deduplicate
  3. Prepare fine-tuning splits with style augmentation
  4. Run SFT on Gemma 4 31B (LoRA)
  5. Run DPO pass (if pairs exist)
  6. Run convo-trainer sessions to generate new training data + scores
  7. Run red team eval
  8. Run blinded A/B eval
  9. Compare to previous best adapter
  10. Promote if improved, rollback if degraded

Usage:
  python3 scripts/training_loop.py                     # full cycle
  python3 scripts/training_loop.py --skip-extract       # skip data extraction
  python3 scripts/training_loop.py --eval-only          # just run eval on current adapter
  python3 scripts/training_loop.py --cycles 3           # run 3 improvement cycles
  python3 scripts/training_loop.py --dry-run            # simulate without training
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = REPO_ROOT / "scripts"
DATA_DIR = REPO_ROOT / "data"
ADAPTER_BASE = Path.home() / ".human" / "training-data" / "adapters"
ADAPTER_PATH = ADAPTER_BASE / "seth-lora"
FINETUNE_DIR = Path.home() / ".human" / "training-data" / "finetune"
HISTORY_PATH = REPO_ROOT / "data" / "training_history.json"


def run_script(script: str, args: list[str] | None = None, check: bool = True) -> int:
    cmd = [sys.executable, str(SCRIPTS / script)]
    if args:
        cmd.extend(args)
    print(f"\n  $ python3 scripts/{script} {' '.join(args or [])}")
    result = subprocess.run(cmd, cwd=str(REPO_ROOT))
    if check and result.returncode != 0:
        print(f"  WARNING: {script} exited with code {result.returncode}")
    return result.returncode


def load_history() -> dict:
    if HISTORY_PATH.exists():
        with open(HISTORY_PATH) as f:
            return json.load(f)
    return {"cycles": [], "best_score": 0, "best_adapter": None}


def save_history(history: dict):
    HISTORY_PATH.parent.mkdir(parents=True, exist_ok=True)
    with open(HISTORY_PATH, "w") as f:
        json.dump(history, f, indent=2)


def get_adapter_version() -> int:
    """Find the current highest adapter version."""
    version = 0
    if ADAPTER_BASE.exists():
        for d in ADAPTER_BASE.iterdir():
            if d.name.startswith("seth-lora-v"):
                try:
                    v = int(d.name.split("-v")[1])
                    version = max(version, v)
                except (ValueError, IndexError):
                    pass
    return version


def read_eval_score(eval_path: Path) -> float:
    """Read overall score from an eval results file."""
    if not eval_path.exists():
        return 0.0
    try:
        with open(eval_path) as f:
            data = json.load(f)
        if "fool_rate" in data:
            return data["fool_rate"]
        if "categories" in data:
            scores = [c["aggregate"]["overall"] for c in data["categories"].values()
                      if "aggregate" in c]
            return sum(scores) / len(scores) if scores else 0
        if "contacts" in data:
            scores = [c["aggregate"]["overall"] for c in data["contacts"].values()
                      if "aggregate" in c]
            return sum(scores) / len(scores) if scores else 0
        return 0.0
    except Exception:
        return 0.0


def phase_extract(args):
    """Phase 1: Extract fresh data from all sources."""
    print(f"\n{'='*60}")
    print(f"  Phase 1: DATA EXTRACTION")
    print(f"{'='*60}")

    run_script("extract_imessage_pairs.py", check=False)
    run_script("extract_facebook_data.py", check=False)
    run_script("extract_apple_photos.py", check=False)


def phase_merge():
    """Phase 2: Merge all data sources."""
    print(f"\n{'='*60}")
    print(f"  Phase 2: MERGE + DEDUPLICATE")
    print(f"{'='*60}")

    run_script("merge_training_sources.py")


def phase_prepare():
    """Phase 3: Prepare fine-tuning data."""
    print(f"\n{'='*60}")
    print(f"  Phase 3: PREPARE FINE-TUNE DATA")
    print(f"{'='*60}")

    run_script("prepare-finetune.py", [
        "--persona", "seth",
        "--include-chatdb",
        "--output", str(FINETUNE_DIR),
    ])


def phase_train(args) -> int:
    """Phase 4: SFT + DPO training."""
    print(f"\n{'='*60}")
    print(f"  Phase 4: FINE-TUNE (SFT + DPO)")
    print(f"{'='*60}")

    train_args = [
        "--data", str(FINETUNE_DIR),
        "--adapter-path", str(ADAPTER_PATH),
        "--iters", str(args.iters),
        "--max-seq-length", "2048",
        "--no-restart-server",
    ]
    if not args.no_dpo:
        train_args.append("--dpo")

    return run_script("finetune-gemma.py", train_args, check=False)


def phase_generate_data(args):
    """Phase 5: Run convo-trainer to generate new training data."""
    print(f"\n{'='*60}")
    print(f"  Phase 5: GENERATE TRAINING DATA (convo-trainer)")
    print(f"{'='*60}")

    timestamp = time.strftime("%Y%m%d-%H%M%S")
    for scenario in ["mixed", "emotional_support", "casual_hangout"]:
        output_dir = f"convo-training-{scenario}-{timestamp}"
        convo_args = [
            "--turns", str(args.convo_turns),
            "--scenario", scenario,
            "--output", output_dir,
        ]
        if args.dry_run:
            convo_args.append("--dry-run")
        convo_args.append("--verbose")

        run_script("convo-trainer.py", convo_args, check=False)


def phase_eval(args) -> dict:
    """Phase 6: Run all evals and return combined score."""
    print(f"\n{'='*60}")
    print(f"  Phase 6: EVALUATION SUITE")
    print(f"{'='*60}")

    scores = {}

    # Red team
    print(f"\n  --- Red Team ---")
    run_script("redteam_persona.py", [
        "--model-url", f"http://127.0.0.1:{args.port}/v1/chat/completions",
    ], check=False)
    redteam_path = DATA_DIR / "eval_redteam_persona.json"
    scores["redteam"] = read_eval_score(redteam_path)
    print(f"  Red team score: {scores['redteam']:.1f}")

    # Per-contact
    print(f"\n  --- Per-Contact ---")
    run_script("eval_per_contact.py", [
        "--model-url", f"http://127.0.0.1:{args.port}/v1/chat/completions",
    ], check=False)
    contact_path = DATA_DIR / "eval_per_contact.json"
    scores["per_contact"] = read_eval_score(contact_path)
    print(f"  Per-contact score: {scores['per_contact']:.1f}")

    # Blinded A/B
    print(f"\n  --- Blinded A/B ---")
    run_script("eval_blinded_ab.py", ["--synthetic"], check=False)
    ab_path = DATA_DIR / "eval_blinded_ab.json"
    scores["blinded_ab"] = read_eval_score(ab_path)
    print(f"  Blinded A/B fool rate: {scores['blinded_ab']:.1f}%")

    # Combined score (weighted)
    combined = (
        scores.get("redteam", 0) * 0.3 +
        scores.get("per_contact", 0) * 0.3 +
        scores.get("blinded_ab", 0) * 0.4
    )
    scores["combined"] = round(combined, 1)

    return scores


def phase_promote_or_rollback(scores: dict, history: dict) -> bool:
    """Phase 7: Promote if improved, rollback if degraded."""
    print(f"\n{'='*60}")
    print(f"  Phase 7: PROMOTE / ROLLBACK")
    print(f"{'='*60}")

    combined = scores["combined"]
    best = history.get("best_score", 0)

    print(f"  Current score:  {combined:.1f}")
    print(f"  Previous best:  {best:.1f}")

    if combined > best:
        print(f"  IMPROVED by {combined - best:.1f} points!")
        print(f"  Promoting adapter...")
        history["best_score"] = combined
        version = get_adapter_version()
        history["best_adapter"] = f"seth-lora-v{version}"
        return True
    elif combined < best - 5:
        print(f"  DEGRADED by {best - combined:.1f} points.")
        print(f"  Rolling back to {history.get('best_adapter', 'none')}...")
        best_adapter = history.get("best_adapter")
        if best_adapter:
            best_path = ADAPTER_BASE / best_adapter
            if best_path.exists():
                if ADAPTER_PATH.exists():
                    shutil.rmtree(ADAPTER_PATH)
                shutil.copytree(best_path, ADAPTER_PATH)
                print(f"  Rolled back to {best_adapter}")
        return False
    else:
        print(f"  Within margin ({abs(combined - best):.1f} points). Keeping new adapter.")
        history["best_score"] = max(combined, best)
        version = get_adapter_version()
        history["best_adapter"] = f"seth-lora-v{version}"
        return True


def run_cycle(args, cycle_num: int, history: dict) -> dict:
    """Run a single improvement cycle."""
    print(f"\n{'#'*60}")
    print(f"  TRAINING CYCLE {cycle_num}")
    print(f"  {time.strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"{'#'*60}")

    cycle_result = {
        "cycle": cycle_num,
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "phases": {},
    }

    t0 = time.time()

    # Extract
    if not args.skip_extract and not args.eval_only:
        phase_extract(args)
        cycle_result["phases"]["extract"] = "done"

    # Merge
    if not args.eval_only:
        phase_merge()
        cycle_result["phases"]["merge"] = "done"

    # Prepare
    if not args.eval_only:
        phase_prepare()
        cycle_result["phases"]["prepare"] = "done"

    # Train
    if not args.eval_only and not args.dry_run:
        rc = phase_train(args)
        cycle_result["phases"]["train"] = "done" if rc == 0 else f"failed ({rc})"
        if rc != 0:
            print(f"\n  Training failed. Skipping eval and promotion.")
            return cycle_result

    # Start server for eval
    if not args.dry_run:
        print(f"\n  Starting MLX server for evaluation...")
        server_cmd = [
            sys.executable, str(SCRIPTS / "mlx-server.py"),
            "--port", str(args.port),
        ]
        adapter_safetensors = ADAPTER_PATH / "adapters.safetensors"
        if adapter_safetensors.exists():
            server_cmd.extend(["--adapter-path", str(ADAPTER_PATH)])
        server = subprocess.Popen(
            server_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
        time.sleep(15)

    # Generate training data
    if not args.eval_only and not args.skip_convo:
        phase_generate_data(args)
        cycle_result["phases"]["generate"] = "done"

    # Eval
    scores = phase_eval(args) if not args.dry_run else {
        "redteam": 60, "per_contact": 60, "blinded_ab": 40, "combined": 52,
    }
    cycle_result["scores"] = scores
    cycle_result["phases"]["eval"] = "done"

    # Kill server
    if not args.dry_run:
        server.terminate()
        try:
            server.wait(timeout=10)
        except subprocess.TimeoutExpired:
            server.kill()

    # Promote or rollback
    if not args.dry_run and not args.eval_only:
        promoted = phase_promote_or_rollback(scores, history)
        cycle_result["promoted"] = promoted

    elapsed = time.time() - t0
    cycle_result["elapsed_minutes"] = round(elapsed / 60, 1)

    # Summary
    print(f"\n{'='*60}")
    print(f"  CYCLE {cycle_num} COMPLETE")
    print(f"{'='*60}")
    print(f"  Time: {elapsed/60:.1f} minutes")
    print(f"  Scores:")
    for k, v in scores.items():
        unit = "%" if k == "blinded_ab" else "/100"
        print(f"    {k}: {v:.1f}{unit}")
    if "promoted" in cycle_result:
        status = "PROMOTED" if cycle_result["promoted"] else "ROLLED BACK"
        print(f"  Adapter: {status}")
    print(f"{'='*60}")

    return cycle_result


def main():
    parser = argparse.ArgumentParser(
        description="Automated training loop for Seth persona",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--cycles", type=int, default=1, help="Number of improvement cycles (default: 1)")
    parser.add_argument("--iters", type=int, default=200, help="SFT training iterations per cycle")
    parser.add_argument("--convo-turns", type=int, default=15, help="Conversation turns per convo-trainer session")
    parser.add_argument("--port", type=int, default=8741, help="MLX server port for eval")
    parser.add_argument("--skip-extract", action="store_true", help="Skip data extraction phase")
    parser.add_argument("--skip-convo", action="store_true", help="Skip convo-trainer data generation")
    parser.add_argument("--no-dpo", action="store_true", help="Skip DPO training pass")
    parser.add_argument("--eval-only", action="store_true", help="Only run evaluation on current adapter")
    parser.add_argument("--dry-run", action="store_true", help="Simulate without actual training/eval")
    args = parser.parse_args()

    print(f"\n{'#'*60}")
    print(f"  h-uman AUTOMATED TRAINING LOOP")
    print(f"{'#'*60}")
    print(f"  Cycles:       {args.cycles}")
    print(f"  SFT iters:    {args.iters}")
    print(f"  Convo turns:  {args.convo_turns}")
    print(f"  DPO:          {'yes' if not args.no_dpo else 'no'}")
    print(f"  Eval only:    {args.eval_only}")
    print(f"  Dry run:      {args.dry_run}")
    print(f"{'#'*60}")

    history = load_history()
    print(f"\n  Previous cycles: {len(history['cycles'])}")
    print(f"  Best score: {history.get('best_score', 0):.1f}")
    print(f"  Best adapter: {history.get('best_adapter', 'none')}")

    for cycle_num in range(1, args.cycles + 1):
        cycle_result = run_cycle(args, len(history["cycles"]) + 1, history)
        history["cycles"].append(cycle_result)
        save_history(history)

        if cycle_num < args.cycles:
            print(f"\n  Waiting 10s before next cycle...")
            time.sleep(10)

    # Final report
    print(f"\n{'#'*60}")
    print(f"  TRAINING LOOP COMPLETE")
    print(f"{'#'*60}")
    print(f"  Cycles run: {args.cycles}")
    print(f"  Best score: {history.get('best_score', 0):.1f}")
    print(f"  Best adapter: {history.get('best_adapter', 'none')}")
    print(f"  History: {HISTORY_PATH}")

    if len(history["cycles"]) >= 2:
        recent = history["cycles"][-args.cycles:]
        scores = [c.get("scores", {}).get("combined", 0) for c in recent if c.get("scores")]
        if len(scores) >= 2:
            trend = scores[-1] - scores[0]
            direction = "improving" if trend > 2 else ("declining" if trend < -2 else "stable")
            print(f"  Trend: {direction} ({'+' if trend > 0 else ''}{trend:.1f})")

    print(f"\n  Next steps:")
    print(f"    ./scripts/start_seth_model.sh      # serve the best adapter")
    print(f"    python3 scripts/training_loop.py --cycles 3  # run more cycles")
    print(f"{'#'*60}\n")


if __name__ == "__main__":
    main()
