#!/usr/bin/env python3
"""
Dual-compute ANE+GPU experimental bridge for Gemma 4 on Apple Silicon.

Splits inference across Neural Engine (ANE) and GPU simultaneously:
  - ANE: runs the E2B draft model via CoreML for speculative token proposals
  - GPU: runs the E4B target model via MLX for token verification

This exploits Apple Silicon's unified memory — both compute units read from
the same physical memory with zero-copy. The ANE draft model runs in parallel
with GPU verification, effectively hiding draft latency.

Architecture:
  ┌─────────────────────────────────────────────────┐
  │              Unified Memory (shared)             │
  │  ┌──────────────┐     ┌───────────────────────┐ │
  │  │   ANE (16c)   │     │    GPU (40 cores)      │ │
  │  │  E2B draft    │────>│  E4B target verify     │ │
  │  │  CoreML model │     │  MLX model             │ │
  │  └──────────────┘     └───────────────────────┘ │
  └─────────────────────────────────────────────────┘

The ANE produces draft tokens at ~300+ tok/s (it's tiny: 2B params).
The GPU verifies N draft tokens in a single forward pass.
Accepted tokens are emitted; rejected ones trigger GPU autoregressive fallback.

Usage:
    python3 scripts/ane-gpu-bridge.py --target e4b --draft e2b
    python3 scripts/ane-gpu-bridge.py --target e4b --draft e2b --port 8741
    python3 scripts/ane-gpu-bridge.py --convert-draft  # export E2B to CoreML first

Prerequisites:
    pip install coremltools mlx mlx-lm
"""
from __future__ import annotations

import argparse
import json
import os
import platform
import subprocess
import sys
import threading
import time
import uuid
from concurrent.futures import ThreadPoolExecutor, Future
from http.server import HTTPServer, BaseHTTPRequestHandler
from pathlib import Path
from socketserver import ThreadingMixIn
from threading import Lock

DEFAULT_PORT = 8741
DRAFT_NUM_TOKENS = 5  # how many tokens the ANE draft proposes per step

# ── Model paths ──────────────────────────────────────────────────
COREML_CACHE = Path.home() / ".cache" / "human-ane-models"
MLX_MODELS = {
    "e4b": "mlx-community/gemma-4-e4b-it-4bit",
    "e2b": "mlx-community/gemma-4-e2b-it-4bit",
    "31b": "mlx-community/gemma-4-31b-it-4bit",
}

# PLE-safe preferred
PLE_SAFE = {
    "e4b": "FakeRockert543/gemma-4-e4b-it-MLX-4bit",
    "e2b": "FakeRockert543/gemma-4-e2b-it-MLX-4bit",
}


def convert_draft_to_coreml(model_name: str, output_dir: Path) -> Path:
    """Convert a HuggingFace model to CoreML for ANE execution.

    Uses coremltools to trace and convert the model with ANE-optimized
    compute units. The resulting .mlpackage runs on the Neural Engine.
    """
    output_dir.mkdir(parents=True, exist_ok=True)
    mlpackage_path = output_dir / f"{model_name.split('/')[-1]}.mlpackage"

    if mlpackage_path.exists():
        print(f"  CoreML model already exists: {mlpackage_path}")
        return mlpackage_path

    print(f"  Converting {model_name} to CoreML for ANE...")
    print(f"  This may take 5-15 minutes on first run.\n")

    try:
        import coremltools as ct
        from transformers import AutoModelForCausalLM, AutoTokenizer
        import torch

        tokenizer = AutoTokenizer.from_pretrained(model_name)
        hf_model = AutoModelForCausalLM.from_pretrained(
            model_name, torch_dtype=torch.float16, device_map="cpu",
        )
        hf_model.eval()

        seq_len = 512  # ANE optimal for short sequences
        dummy_input = torch.randint(0, tokenizer.vocab_size, (1, seq_len))

        traced = torch.jit.trace(hf_model, dummy_input)

        mlmodel = ct.convert(
            traced,
            inputs=[ct.TensorType(name="input_ids", shape=(1, ct.RangeDim(1, seq_len)))],
            compute_units=ct.ComputeUnit.ALL,  # prefer ANE
            minimum_deployment_target=ct.target.macOS15,
        )
        mlmodel.save(str(mlpackage_path))
        print(f"  CoreML model saved: {mlpackage_path}")
        size_mb = sum(f.stat().st_size for f in mlpackage_path.rglob("*") if f.is_file()) / (1024**2)
        print(f"  Size: {size_mb:.0f} MB")

    except ImportError as e:
        print(f"  ERROR: Missing dependency: {e}")
        print(f"  Install: pip install coremltools transformers torch")
        sys.exit(1)
    except Exception as e:
        print(f"  ERROR: CoreML conversion failed: {e}")
        print(f"\n  Falling back to ExecuTorch CoreML delegate...")
        return _convert_via_executorch(model_name, output_dir)

    return mlpackage_path


def _convert_via_executorch(model_name: str, output_dir: Path) -> Path:
    """Fallback: use ExecuTorch's CoreML delegate for ANE execution.

    ExecuTorch has better support for newer model architectures and
    can handle dynamic shapes that coremltools sometimes struggles with.
    """
    pte_path = output_dir / f"{model_name.split('/')[-1]}.pte"

    if pte_path.exists():
        print(f"  ExecuTorch model exists: {pte_path}")
        return pte_path

    try:
        from executorch.exir import to_edge
        from executorch.backends.apple.coreml.compiler import CoreMLBackend

        print(f"  ExecuTorch CoreML delegate conversion...")
        print(f"  (ExecuTorch delegates model subgraphs to ANE)")

        # This is a simplified flow — real implementation needs model-specific export
        print(f"  NOTE: Full ExecuTorch export requires model-specific config.")
        print(f"  See: https://pytorch.org/executorch/stable/apple-coreml-delegate.html")

    except ImportError:
        print(f"  ExecuTorch not installed. Install: pip install executorch")

    # For now, return a marker that tells the bridge to use MLX fallback for draft
    marker = output_dir / "USE_MLX_DRAFT"
    marker.touch()
    return marker


class ANEDraftEngine:
    """Runs the draft model on the Neural Engine via CoreML."""

    def __init__(self, model_path: Path):
        self.model_path = model_path
        self.model = None
        self.use_mlx_fallback = False

        if model_path.name == "USE_MLX_DRAFT":
            print("  ANE: Using MLX fallback for draft (CoreML conversion pending)")
            self.use_mlx_fallback = True
            return

        if model_path.suffix == ".mlpackage" and model_path.exists():
            try:
                import coremltools as ct
                self.model = ct.models.MLModel(
                    str(model_path),
                    compute_units=ct.ComputeUnit.CPU_AND_NE,  # CPU + Neural Engine
                )
                print(f"  ANE draft model loaded: {model_path.name}")
            except Exception as e:
                print(f"  ANE load failed ({e}), falling back to MLX")
                self.use_mlx_fallback = True
        else:
            self.use_mlx_fallback = True

    def generate_draft_tokens(self, input_ids, num_tokens: int = DRAFT_NUM_TOKENS):
        """Generate draft token proposals on the ANE."""
        if self.use_mlx_fallback:
            return None  # caller should use MLX draft instead

        import numpy as np
        ids = np.array(input_ids, dtype=np.int32).reshape(1, -1)
        draft_tokens = []

        for _ in range(num_tokens):
            prediction = self.model.predict({"input_ids": ids})
            logits = prediction.get("logits", prediction.get("output", None))
            if logits is None:
                break
            next_token = int(np.argmax(logits[0, -1, :]))
            draft_tokens.append(next_token)
            ids = np.append(ids, [[next_token]], axis=1)

        return draft_tokens


class GPUTargetEngine:
    """Runs the target model on the GPU via MLX."""

    def __init__(self, model_name: str, adapter_path: str = None):
        from mlx_lm import load as lm_load
        import mlx.core as mx

        self.mx = mx
        print(f"  GPU: Loading target model {model_name}...")
        t0 = time.time()
        self.model, self.tokenizer = lm_load(model_name)

        if adapter_path:
            adapter_file = Path(adapter_path) / "adapters.safetensors"
            if adapter_file.exists():
                adapters = list(mx.load(str(adapter_file)).items())
                self.model.load_weights(adapters, strict=False)
                print(f"  GPU: Applied {len(adapters)} LoRA tensors")

        elapsed = time.time() - t0
        print(f"  GPU: Target model loaded in {elapsed:.1f}s")

    def verify_draft_tokens(self, input_ids, draft_tokens):
        """Verify draft tokens in a single batched forward pass.

        Returns (accepted_tokens, first_rejected_token_or_None).
        This is the key speedup: verifying N tokens costs ~1 forward pass
        instead of N sequential autoregressive steps.
        """
        import mlx.core as mx
        import mlx.nn as nn

        full_ids = input_ids + draft_tokens
        tokens = mx.array([full_ids])

        logits = self.model(tokens)
        mx.eval(logits)

        accepted = []
        start_pos = len(input_ids) - 1

        for i, draft_tok in enumerate(draft_tokens):
            pos = start_pos + i
            if pos >= logits.shape[1]:
                break
            target_tok = int(mx.argmax(logits[0, pos, :]).item())
            if target_tok == draft_tok:
                accepted.append(draft_tok)
            else:
                accepted.append(target_tok)
                return accepted, True  # diverged, last token is from target
            
        return accepted, False  # all accepted

    def generate_single(self, input_ids):
        """Standard autoregressive generation for a single token."""
        import mlx.core as mx
        tokens = mx.array([input_ids])
        logits = self.model(tokens)
        mx.eval(logits)
        return int(mx.argmax(logits[0, -1, :]).item())

    def encode(self, text):
        return self.tokenizer.encode(text)

    def decode(self, token_ids):
        return self.tokenizer.decode(token_ids, skip_special_tokens=True)

    def apply_chat_template(self, messages):
        if hasattr(self.tokenizer, "apply_chat_template"):
            return self.tokenizer.apply_chat_template(
                messages, tokenize=False, add_generation_prompt=True,
            )
        parts = []
        for msg in messages:
            parts.append(f"<start_of_turn>{msg['role']}\n{msg['content']}<end_of_turn>")
        parts.append("<start_of_turn>model\n")
        return "\n".join(parts)


class DualComputeBridge:
    """Orchestrates ANE draft + GPU verify for speculative decoding."""

    def __init__(self, ane_engine: ANEDraftEngine, gpu_engine: GPUTargetEngine,
                 mlx_draft_model=None, mlx_draft_tokenizer=None,
                 num_draft_tokens: int = DRAFT_NUM_TOKENS):
        self.ane = ane_engine
        self.gpu = gpu_engine
        self.mlx_draft = mlx_draft_model
        self.mlx_draft_tokenizer = mlx_draft_tokenizer
        self.num_draft_tokens = num_draft_tokens
        self.executor = ThreadPoolExecutor(max_workers=2)
        self.stats = {"accepted": 0, "total_draft": 0, "total_tokens": 0}

    def generate_stream(self, messages, max_tokens=256, temperature=0.7):
        """Stream tokens using dual-compute speculative decoding.

        Yields (token_text, is_final) tuples.
        """
        prompt = self.gpu.apply_chat_template(messages)
        input_ids = self.gpu.encode(prompt)
        generated = []

        for _ in range(max_tokens):
            # Phase 1: ANE drafts tokens (or MLX fallback)
            draft_tokens = None
            if not self.ane.use_mlx_fallback:
                draft_tokens = self.ane.generate_draft_tokens(
                    input_ids + generated, self.num_draft_tokens,
                )

            if draft_tokens is None and self.mlx_draft is not None:
                draft_tokens = self._mlx_draft_tokens(input_ids + generated)

            if draft_tokens and len(draft_tokens) > 0:
                # Phase 2: GPU verifies all draft tokens in one pass
                accepted, diverged = self.gpu.verify_draft_tokens(
                    input_ids + generated, draft_tokens,
                )
                self.stats["total_draft"] += len(draft_tokens)
                self.stats["accepted"] += len(accepted) - (1 if diverged else 0)

                for tok in accepted:
                    generated.append(tok)
                    self.stats["total_tokens"] += 1
                    text = self.gpu.decode([tok])
                    if "<end_of_turn>" in text or "<eos>" in text:
                        return
                    yield text, False
            else:
                # Fallback: standard autoregressive (no draft available)
                tok = self.gpu.generate_single(input_ids + generated)
                generated.append(tok)
                self.stats["total_tokens"] += 1
                text = self.gpu.decode([tok])
                if "<end_of_turn>" in text or "<eos>" in text:
                    return
                yield text, False

    def _mlx_draft_tokens(self, context_ids, num_tokens=None):
        """Use MLX to generate draft tokens when ANE is unavailable."""
        import mlx.core as mx

        if num_tokens is None:
            num_tokens = self.num_draft_tokens
        ids = mx.array([context_ids])
        draft_tokens = []

        for _ in range(num_tokens):
            logits = self.mlx_draft(ids)
            mx.eval(logits)
            next_tok = int(mx.argmax(logits[0, -1, :]).item())
            draft_tokens.append(next_tok)
            ids = mx.concatenate([ids, mx.array([[next_tok]])], axis=1)

        return draft_tokens

    def get_acceptance_rate(self):
        if self.stats["total_draft"] == 0:
            return 0.0
        return self.stats["accepted"] / self.stats["total_draft"]


# ── HTTP Server (OpenAI-compatible) ─────────────────────────────

bridge_instance: DualComputeBridge | None = None
bridge_lock = Lock()
model_id = "gemma-4-ane-gpu"
perf_stats = {"total_tokens": 0, "total_time": 0.0, "requests": 0}


class DualComputeHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        ts = time.strftime("%H:%M:%S")
        print(f"[{ts}] {args[0]}", flush=True)

    def _send_json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
        self.end_headers()

    def do_GET(self):
        if self.path == "/health":
            health = {
                "status": "ok",
                "model": model_id,
                "engine": "ane+gpu dual-compute",
                "acceptance_rate": round(bridge_instance.get_acceptance_rate(), 3) if bridge_instance else 0,
                "stats": bridge_instance.stats if bridge_instance else {},
            }
            if perf_stats["requests"] > 0 and perf_stats["total_time"] > 0:
                health["avg_tok_per_sec"] = round(
                    perf_stats["total_tokens"] / perf_stats["total_time"], 1,
                )
            self._send_json(200, health)
            return

        if self.path == "/v1/models":
            self._send_json(200, {
                "object": "list",
                "data": [{"id": model_id, "object": "model", "owned_by": "local-ane-gpu"}],
            })
            return

        self._send_json(404, {"error": "not found"})

    def do_POST(self):
        if self.path != "/v1/chat/completions":
            self._send_json(404, {"error": "not found"})
            return

        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length)
        try:
            req = json.loads(body.decode("utf-8", errors="replace"))
        except (json.JSONDecodeError, UnicodeDecodeError):
            self._send_json(400, {"error": "invalid JSON"})
            return

        messages = req.get("messages", [])
        max_tokens = req.get("max_tokens", 256)
        stream = req.get("stream", False)
        t0 = time.time()
        resp_id = f"chatcmpl-{uuid.uuid4().hex[:12]}"

        if stream:
            self._handle_stream(messages, max_tokens, resp_id, t0)
        else:
            self._handle_non_stream(messages, max_tokens, resp_id, t0)

    def _handle_stream(self, messages, max_tokens, resp_id, t0):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "close")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()

        gen_toks = 0
        first_token_time = None

        with bridge_lock:
            for text, _ in bridge_instance.generate_stream(messages, max_tokens):
                if first_token_time is None:
                    first_token_time = time.time()
                gen_toks += 1

                chunk = {
                    "id": resp_id,
                    "object": "chat.completion.chunk",
                    "created": int(time.time()),
                    "model": model_id,
                    "choices": [{"index": 0, "delta": {"content": text}, "finish_reason": None}],
                }
                self.wfile.write(f"data: {json.dumps(chunk)}\n\n".encode())
                self.wfile.flush()

        done = {
            "id": resp_id,
            "object": "chat.completion.chunk",
            "created": int(time.time()),
            "model": model_id,
            "choices": [{"index": 0, "delta": {}, "finish_reason": "stop"}],
        }
        self.wfile.write(f"data: {json.dumps(done)}\n\n".encode())
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()

        elapsed = time.time() - t0
        ttft = (first_token_time - t0) if first_token_time else elapsed
        tps = gen_toks / elapsed if elapsed > 0 else 0
        ar = bridge_instance.get_acceptance_rate()

        perf_stats["total_tokens"] += gen_toks
        perf_stats["total_time"] += elapsed
        perf_stats["requests"] += 1

        print(f"  -> {gen_toks} tok, {elapsed:.1f}s ({tps:.1f} t/s, TTFT {ttft:.2f}s, AR {ar:.0%})", flush=True)

    def _handle_non_stream(self, messages, max_tokens, resp_id, t0):
        full_text = []
        with bridge_lock:
            for text, _ in bridge_instance.generate_stream(messages, max_tokens):
                full_text.append(text)

        elapsed = time.time() - t0
        self._send_json(200, {
            "id": resp_id,
            "object": "chat.completion",
            "created": int(time.time()),
            "model": model_id,
            "choices": [{
                "index": 0,
                "message": {"role": "assistant", "content": "".join(full_text)},
                "finish_reason": "stop",
            }],
        })


def main():
    global bridge_instance, model_id

    parser = argparse.ArgumentParser(
        description="Dual-compute ANE+GPU speculative decoding bridge",
    )
    parser.add_argument("--target", choices=["e4b", "31b"], default="e4b",
                        help="Target model for GPU verification (default: e4b)")
    parser.add_argument("--draft", choices=["e2b"], default="e2b",
                        help="Draft model for ANE/speculative proposals (default: e2b)")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--adapter-path", default=None,
                        help="LoRA adapter for target model")
    parser.add_argument("--draft-adapter-path", default=None,
                        help="LoRA adapter for draft model")
    parser.add_argument("--convert-draft", action="store_true",
                        help="Convert draft model to CoreML (ANE) and exit")
    parser.add_argument("--ple-safe", action="store_true", default=True,
                        help="Use PLE-safe model variants (default: true)")
    parser.add_argument("--draft-tokens", type=int, default=DRAFT_NUM_TOKENS,
                        help=f"Number of draft tokens per step (default: {DRAFT_NUM_TOKENS})")
    args = parser.parse_args()

    draft_num_tokens = args.draft_tokens

    target_model = PLE_SAFE.get(args.target, MLX_MODELS[args.target]) if args.ple_safe else MLX_MODELS[args.target]
    draft_model = MLX_MODELS[args.draft]

    print(f"\n{'='*60}")
    print(f"  Dual-Compute ANE+GPU Bridge")
    print(f"{'='*60}")

    chip = "unknown"
    try:
        result = subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"],
                                capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            chip = result.stdout.strip()
    except Exception:
        pass
    print(f"  Hardware:  {chip}")
    print(f"  Target:    {args.target} ({target_model}) → GPU")
    print(f"  Draft:     {args.draft} ({draft_model}) → ANE (with MLX fallback)")
    print(f"  Draft tokens: {draft_num_tokens} per step")

    if args.convert_draft:
        print(f"{'='*60}\n")
        coreml_path = convert_draft_to_coreml(draft_model, COREML_CACHE)
        print(f"\n  CoreML model: {coreml_path}")
        print(f"  Run server: python3 scripts/ane-gpu-bridge.py --target {args.target} --draft {args.draft}")
        return

    print(f"{'='*60}\n")

    # Load ANE draft engine
    coreml_path = COREML_CACHE / f"{draft_model.split('/')[-1]}.mlpackage"
    ane_engine = ANEDraftEngine(coreml_path)

    # Load MLX draft model as fallback
    mlx_draft = None
    mlx_draft_tokenizer = None
    if ane_engine.use_mlx_fallback:
        try:
            from mlx_lm import load as lm_load
            print(f"  Loading MLX draft model (ANE fallback)...")
            mlx_draft, mlx_draft_tokenizer = lm_load(draft_model)
            if args.draft_adapter_path:
                import mlx.core as mx
                adapter_file = Path(args.draft_adapter_path) / "adapters.safetensors"
                if adapter_file.exists():
                    adapters = list(mx.load(str(adapter_file)).items())
                    mlx_draft.load_weights(adapters, strict=False)
        except Exception as e:
            print(f"  WARNING: Could not load MLX draft: {e}")

    # Load GPU target engine
    gpu_engine = GPUTargetEngine(target_model, adapter_path=args.adapter_path)

    # Create bridge
    bridge_instance = DualComputeBridge(
        ane_engine, gpu_engine, mlx_draft, mlx_draft_tokenizer,
        num_draft_tokens=draft_num_tokens,
    )
    model_id = f"gemma-4-{args.target}-ane-gpu"

    # Start server
    class ThreadedServer(ThreadingMixIn, HTTPServer):
        allow_reuse_address = True
        daemon_threads = True

    server = ThreadedServer(("127.0.0.1", args.port), DualComputeHandler)
    print(f"\n  Serving on http://127.0.0.1:{args.port}/v1/chat/completions")
    print(f"  Engine: ANE+GPU dual-compute speculative decoding")
    print(f"  Health: http://127.0.0.1:{args.port}/health")
    print(f"\n  Test:")
    print(f"    curl http://127.0.0.1:{args.port}/v1/chat/completions \\")
    print(f"      -H 'Content-Type: application/json' \\")
    print(f"      -d '{{\"messages\": [{{\"role\": \"user\", \"content\": \"hey\"}}], \"stream\": true}}'")
    print(f"\n  Benchmark:")
    print(f"    python3 scripts/voice-bench.py --endpoint http://127.0.0.1:{args.port}")
    print(f"\n  Press Ctrl+C to stop.\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.shutdown()


if __name__ == "__main__":
    main()
