#!/usr/bin/env python3
"""
Sonata Bridge — connect pocket-voice's real-time audio pipeline to h-uman's
fine-tuned Gemma model via the MLX server.

This script acts as a bridge between two systems:
  1. Sonata (pocket-voice): handles CoreAudio, STT, TTS, VAD, barge-in
  2. h-uman MLX server: serves fine-tuned Gemma with speculative decoding

Architecture:
  Sonata (mic→STT→text) → Bridge → MLX Server (text→SSE→text) → Bridge → Sonata (text→TTS→speaker)

The bridge replaces Sonata's default LLM backend (Claude/Gemini) with the local
MLX server, giving you:
  - Your fine-tuned personality (LoRA adapter)
  - Speculative decoding (~2x speedup)
  - Zero network latency for inference
  - Full Sonata pipeline: barge-in, 3-signal EOU, speculative prefill, spatial audio

Usage:
  # Start MLX server first
  python3 scripts/mlx-server.py --model mlx-community/gemma-4-e4b-it-4bit \\
    --adapter-path ~/.human/training-data/adapters/seth-lora-e4b --realtime

  # Then run Sonata with the bridge
  python3 scripts/sonata-bridge.py

  # Or point to a specific Sonata install
  python3 scripts/sonata-bridge.py --sonata-dir /Users/sethford/Documents/pocket-voice

Prerequisites:
  - Sonata built: cd /Users/sethford/Documents/pocket-voice && make
  - MLX server running: python3 scripts/mlx-server.py
"""
from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

DEFAULT_MLX_ENDPOINT = "http://127.0.0.1:8741"
DEFAULT_SONATA_DIR = str(Path.home() / "Documents" / "pocket-voice")
DEFAULT_SYSTEM_PROMPT = (
    "You are Seth's personal AI assistant. Be conversational, warm, and concise. "
    "Respond naturally as if speaking aloud — keep responses under 2-3 sentences "
    "unless the topic needs more depth."
)


def check_mlx_server(endpoint: str) -> dict | None:
    """Check if the MLX server is running and healthy."""
    try:
        req = urllib.request.Request(f"{endpoint}/health", method="GET")
        resp = urllib.request.urlopen(req, timeout=5)
        return json.loads(resp.read())
    except Exception:
        return None


def check_sonata_binary(sonata_dir: str) -> str | None:
    """Check if the Sonata binary exists."""
    binary = os.path.join(sonata_dir, "sonata")
    if os.path.isfile(binary) and os.access(binary, os.X_OK):
        return binary
    return None


def generate_sonata_config(endpoint: str, system_prompt: str, voice: str,
                           extra_args: list[str]) -> dict:
    """Generate a Sonata-compatible LLM config pointing to the MLX server."""
    return {
        "llm_backend": "openai_compatible",
        "llm_endpoint": f"{endpoint}/v1/chat/completions",
        "llm_model": "local-gemma",
        "system_prompt": system_prompt,
        "voice": voice,
        "stream": True,
        "max_tokens": 256,
        "temperature": 0.7,
    }


def run_bridge(args):
    print(f"\n{'='*60}")
    print(f"  Sonata ↔ h-uman MLX Bridge")
    print(f"{'='*60}\n")

    health = check_mlx_server(args.endpoint)
    if not health:
        print(f"ERROR: MLX server not responding at {args.endpoint}")
        print(f"Start it with:")
        print(f"  python3 scripts/mlx-server.py --model mlx-community/gemma-4-e4b-it-4bit --realtime")
        return 1

    print(f"  MLX server: {args.endpoint}")
    print(f"  Model:      {health.get('model', 'unknown')}")
    if health.get("speculative_decoding"):
        print(f"  Speculative: ENABLED (draft tokens: {health.get('draft_tokens', '?')})")
    if health.get("hardware"):
        hw = health["hardware"]
        print(f"  Hardware:   {hw.get('chip', 'unknown')}")
        if hw.get("has_tensor_ops"):
            print(f"  TensorOps:  ENABLED")
    if health.get("adapter"):
        print(f"  Adapter:    {health['adapter']}")
    if health.get("avg_tok_per_sec"):
        print(f"  Avg speed:  {health['avg_tok_per_sec']} tok/s")

    binary = check_sonata_binary(args.sonata_dir)
    if not binary:
        print(f"\n  WARNING: Sonata binary not found at {args.sonata_dir}/sonata")
        print(f"  Build it: cd {args.sonata_dir} && make")
        print(f"\n  Running in standalone mode (bridge only, no audio pipeline)")
        print(f"  The MLX server is accessible at: {args.endpoint}/v1/chat/completions")
        print(f"\n  Test with curl:")
        print(f"  curl -s {args.endpoint}/v1/chat/completions \\")
        print(f'    -H "Content-Type: application/json" \\')
        prompt_preview = args.system_prompt[:50].replace('"', '\\"')
        print(f'    -d \'{{"messages":[{{"role":"system","content":"{prompt_preview}..."}},{{"role":"user","content":"hey whats up"}}],"stream":true}}\'')

        return 0

    print(f"\n  Sonata:     {binary}")
    print(f"  Voice mode: Full-duplex with barge-in")
    print(f"{'='*60}\n")

    config = generate_sonata_config(args.endpoint, args.system_prompt,
                                    args.voice, args.sonata_args)

    config_path = os.path.join(args.sonata_dir, ".bridge-config.json")
    with open(config_path, "w") as f:
        json.dump(config, f, indent=2)

    sonata_cmd = [binary]

    sonata_cmd.extend([
        "--llm", "claude",
        "--llm-model", "local-gemma",
        "--system", args.system_prompt,
    ])

    sonata_cmd.extend(args.sonata_args)

    env = os.environ.copy()
    env["SONATA_LLM_ENDPOINT"] = f"{args.endpoint}/v1/chat/completions"
    env["SONATA_LLM_STREAM"] = "true"

    print(f"  Launching Sonata with local Gemma backend...")
    print(f"  Speak naturally. Barge-in supported. Ctrl+C to stop.\n")

    try:
        proc = subprocess.Popen(sonata_cmd, env=env, cwd=args.sonata_dir)
        proc.wait()
        return proc.returncode
    except KeyboardInterrupt:
        print("\n  Shutting down Sonata...")
        proc.send_signal(signal.SIGTERM)
        proc.wait(timeout=5)
        return 0


def main():
    parser = argparse.ArgumentParser(
        description="Bridge Sonata voice pipeline to h-uman's fine-tuned Gemma",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--endpoint", default=DEFAULT_MLX_ENDPOINT,
                        help=f"MLX server endpoint (default: {DEFAULT_MLX_ENDPOINT})")
    parser.add_argument("--sonata-dir", default=DEFAULT_SONATA_DIR,
                        help=f"Path to pocket-voice/Sonata project (default: {DEFAULT_SONATA_DIR})")
    parser.add_argument("--system-prompt", default=DEFAULT_SYSTEM_PROMPT,
                        help="System prompt for the voice assistant")
    parser.add_argument("--voice", default="piper",
                        help="TTS voice engine (default: piper)")
    parser.add_argument("sonata_args", nargs="*",
                        help="Additional arguments passed to Sonata binary")
    args = parser.parse_args()

    sys.exit(run_bridge(args))


if __name__ == "__main__":
    main()
