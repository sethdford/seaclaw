#!/usr/bin/env python3
"""
OpenAI-compatible API server for local MLX model inference.
Serves Gemma 4 31B (or any mlx-vlm model) on http://127.0.0.1:8741/v1

Usage:
    python3 scripts/mlx-server.py [--model mlx-community/gemma-4-31b-it-4bit] [--port 8741]

Features:
    - True token-by-token SSE streaming via mlx_vlm.stream_generate
    - TurboQuant KV cache compression (4.6x smaller, ~0.98x FP16 speed)
    - Cross-turn prompt caching (system prompt KV reused across requests)

The server exposes:
    POST /v1/chat/completions  — OpenAI-compatible chat endpoint (supports stream:true)
    GET  /v1/models            — list available models
    GET  /health               — health check
"""

import argparse
import json
import os
import time
import uuid
from http.server import HTTPServer, BaseHTTPRequestHandler
from socketserver import ThreadingMixIn
from threading import Lock
import socket

DEFAULT_MODEL = "mlx-community/gemma-4-31b-it-4bit"
DEFAULT_PORT = 8741

model = None
processor = None
config = None
model_lock = Lock()
model_id = None

# TurboQuant settings
kv_bits = None
kv_quant_scheme = "uniform"

# Cross-turn prompt cache — reuses KV state when the prompt prefix matches
prompt_cache_state = None

STOP_STRINGS = ("<end_of_turn>", "<eos>")


adapter_path_global = None

def load_model(model_name, adapter_path=None):
    global model, processor, config, model_id, adapter_path_global
    from mlx_vlm import load as vlm_load
    from mlx_vlm.utils import load_config as vlm_load_config

    adapter_path_global = adapter_path
    label = model_name
    if adapter_path:
        label += f" + LoRA adapter ({adapter_path})"
    print(f"Loading {label}...", flush=True)
    t0 = time.time()

    if adapter_path:
        # Load base model without adapter first, then apply mlx_lm-trained adapter
        model, processor = vlm_load(model_name)
        import mlx.core as mx
        from pathlib import Path
        adapter_file = Path(adapter_path) / "adapters.safetensors"
        if adapter_file.exists():
            import mlx.nn as nn
            adapters = list(mx.load(str(adapter_file)).items())
            model.load_weights(adapters, strict=False)
            print(f"  Applied {len(adapters)} LoRA weight tensors from {adapter_file}", flush=True)
    else:
        model, processor = vlm_load(model_name)

    config = vlm_load_config(model_name)
    model_id = model_name.split("/")[-1] if "/" in model_name else model_name
    elapsed = time.time() - t0
    adapter_tag = " (with LoRA adapter)" if adapter_path else ""
    print(f"Model loaded in {elapsed:.1f}s{adapter_tag} — ready to serve", flush=True)


def _extract_content(content):
    """Extract text and image data from a message content field.
    Content can be a string or an array of parts (OpenAI vision format)."""
    if isinstance(content, str):
        return content, []
    if not isinstance(content, list):
        return str(content) if content else "", []

    text_parts = []
    images = []
    for part in content:
        ptype = part.get("type", "")
        if ptype == "text":
            text_parts.append(part.get("text", ""))
        elif ptype == "image_url":
            url = part.get("image_url", {}).get("url", "")
            if url.startswith("data:"):
                import base64
                from io import BytesIO
                try:
                    header, b64data = url.split(",", 1)
                    raw = base64.b64decode(b64data)
                    from PIL import Image
                    img = Image.open(BytesIO(raw)).convert("RGB")
                    images.append(img)
                except Exception as e:
                    text_parts.append(f"[Image decode failed: {e}]")
            elif url.startswith("http"):
                text_parts.append(f"[Image URL: {url}]")
    return " ".join(text_parts), images


def prepare_prompt(messages):
    """Format messages into the model's chat template."""
    from mlx_vlm.prompt_utils import apply_chat_template

    system_parts = []
    conversation = []
    all_images = []
    for msg in messages:
        role = msg.get("role", "user")
        text, images = _extract_content(msg.get("content", ""))
        all_images.extend(images)
        if role == "system":
            system_parts.append(text)
        else:
            conversation.append({"role": role, "content": text})

    if system_parts:
        system_text = "\n".join(system_parts)
        if conversation:
            first_content = conversation[0].get("content", "")
            prompt_text = f"System: {system_text}\n\n{first_content}"
        else:
            prompt_text = f"System: {system_text}"
    elif conversation:
        prompt_text = conversation[-1].get("content", "")
    else:
        prompt_text = ""

    return apply_chat_template(processor, config, prompt_text, num_images=len(all_images)), all_images


def strip_stop_tokens(text):
    for stop in STOP_STRINGS:
        idx = text.find(stop)
        if idx != -1:
            return text[:idx], True
    return text, False


def _stream_kwargs():
    """Build extra kwargs for TurboQuant and prompt caching."""
    extra = {}
    if kv_bits is not None:
        extra["kv_bits"] = kv_bits
        extra["kv_quant_scheme"] = kv_quant_scheme
    if prompt_cache_state is not None:
        extra["prompt_cache_state"] = prompt_cache_state
    return extra


def generate_response(messages, max_tokens=256, temperature=0.7):
    """Non-streaming: generate the full response at once."""
    from mlx_vlm import generate as vlm_generate

    formatted, images = prepare_prompt(messages)
    extra = _stream_kwargs()
    if images:
        extra["images"] = images
    result = vlm_generate(
        model, processor, formatted,
        max_tokens=max_tokens, verbose=False,
        **extra,
    )
    text, _ = strip_stop_tokens(result.text)
    return text.strip(), result.prompt_tokens, result.generation_tokens


def stream_response(messages, max_tokens=256, temperature=0.7):
    """Streaming generator: yield (text_chunk, prompt_toks, gen_toks) per token."""
    from mlx_vlm import stream_generate as vlm_stream_generate

    formatted, images = prepare_prompt(messages)
    prompt_toks = 0
    gen_toks = 0
    extra = _stream_kwargs()
    if images:
        extra["images"] = images

    for chunk in vlm_stream_generate(
        model, processor, formatted,
        max_tokens=max_tokens, temperature=temperature,
        **extra,
    ):
        prompt_toks = getattr(chunk, "prompt_tokens", prompt_toks)
        gen_toks = getattr(chunk, "generation_tokens", gen_toks)
        text = chunk.text or ""

        cleaned, hit_stop = strip_stop_tokens(text)
        if cleaned:
            yield cleaned, prompt_toks, gen_toks
        if hit_stop:
            return

    return


class ChatHandler(BaseHTTPRequestHandler):
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
            cache_info = {}
            if kv_bits is not None:
                cache_info["kv_bits"] = kv_bits
                cache_info["kv_quant_scheme"] = kv_quant_scheme
            if prompt_cache_state is not None and prompt_cache_state.token_ids is not None:
                cache_info["cached_tokens"] = len(prompt_cache_state.token_ids)
            if adapter_path_global:
                cache_info["adapter"] = adapter_path_global
            self._send_json(200, {"status": "ok", "model": model_id, **cache_info})
            return

        if self.path == "/v1/models":
            self._send_json(200, {
                "object": "list",
                "data": [{
                    "id": model_id,
                    "object": "model",
                    "owned_by": "local-mlx",
                }]
            })
            return

        self._send_json(404, {"error": "not found"})

    def _handle_stream(self, req, resp_id, t0):
        messages = req.get("messages", [])
        max_tokens = req.get("max_tokens", 256)
        temperature = req.get("temperature", 0.7)

        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "close")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()

        full_text = []
        prompt_toks = 0
        gen_toks = 0
        first_token_time = None

        with model_lock:
            for text, pt, gt in stream_response(messages, max_tokens, temperature):
                if first_token_time is None:
                    first_token_time = time.time()
                prompt_toks = pt
                gen_toks = gt
                full_text.append(text)

                chunk = {
                    "id": resp_id,
                    "object": "chat.completion.chunk",
                    "created": int(time.time()),
                    "model": model_id,
                    "choices": [{
                        "index": 0,
                        "delta": {"content": text},
                        "finish_reason": None,
                    }],
                }
                self.wfile.write(f"data: {json.dumps(chunk)}\n\n".encode())
                self.wfile.flush()

        done_chunk = {
            "id": resp_id,
            "object": "chat.completion.chunk",
            "created": int(time.time()),
            "model": model_id,
            "choices": [{"index": 0, "delta": {}, "finish_reason": "stop"}],
            "usage": {
                "prompt_tokens": prompt_toks,
                "completion_tokens": gen_toks,
                "total_tokens": prompt_toks + gen_toks,
            },
        }
        self.wfile.write(f"data: {json.dumps(done_chunk)}\n\n".encode())
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()

        elapsed = time.time() - t0
        ttft = (first_token_time - t0) if first_token_time else elapsed
        combined = "".join(full_text)
        preview = combined[:60].replace("\n", " ")
        tps = gen_toks / elapsed if elapsed > 0 else 0
        cache_tag = f" [TQ{kv_bits}b]" if kv_bits is not None else ""
        reused = ""
        if prompt_cache_state is not None and prompt_cache_state.token_ids is not None:
            reused = f" [cache:{len(prompt_cache_state.token_ids)} toks]"
        print(f"  -> {gen_toks} tokens in {elapsed:.1f}s ({tps:.1f} tok/s, TTFT {ttft:.1f}s){cache_tag}{reused} | {preview}...", flush=True)

    def _handle_non_stream(self, req, resp_id, t0):
        messages = req.get("messages", [])
        max_tokens = req.get("max_tokens", 256)
        temperature = req.get("temperature", 0.7)

        with model_lock:
            text, prompt_toks, gen_toks = generate_response(messages, max_tokens, temperature)

        elapsed = time.time() - t0
        self._send_json(200, {
            "id": resp_id,
            "object": "chat.completion",
            "created": int(time.time()),
            "model": model_id,
            "choices": [{
                "index": 0,
                "message": {"role": "assistant", "content": text},
                "finish_reason": "stop",
            }],
            "usage": {
                "prompt_tokens": prompt_toks,
                "completion_tokens": gen_toks,
                "total_tokens": prompt_toks + gen_toks,
            },
        })

        preview = text[:60].replace("\n", " ")
        tps = gen_toks / elapsed if elapsed > 0 else 0
        print(f"  -> {gen_toks} tokens in {elapsed:.1f}s ({tps:.1f} tok/s) | {preview}...", flush=True)

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

        t0 = time.time()
        resp_id = f"chatcmpl-{uuid.uuid4().hex[:12]}"

        if req.get("stream", False):
            self._handle_stream(req, resp_id, t0)
        else:
            self._handle_non_stream(req, resp_id, t0)


def main():
    global kv_bits, kv_quant_scheme, prompt_cache_state

    parser = argparse.ArgumentParser(description="MLX OpenAI-compatible model server")
    parser.add_argument("--model", default=os.environ.get("MLX_MODEL", DEFAULT_MODEL))
    parser.add_argument("--port", type=int, default=int(os.environ.get("MLX_PORT", DEFAULT_PORT)))
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument(
        "--adapter-path", default=os.environ.get("MLX_ADAPTER_PATH", None),
        help="Path to LoRA adapter directory (e.g. from finetune-gemma.py). "
             "Also settable via MLX_ADAPTER_PATH env var.",
    )
    parser.add_argument(
        "--kv-bits", type=float, default=None,
        help="KV cache quantization bits. Use 3 for TurboQuant 3-bit (4.6x compression). "
             "Omit to use full FP16 KV cache.",
    )
    parser.add_argument(
        "--no-prompt-cache", action="store_true",
        help="Disable cross-turn prompt cache reuse.",
    )
    args = parser.parse_args()

    if args.kv_bits is not None:
        kv_bits = args.kv_bits
        kv_quant_scheme = "turboquant"
        from mlx_vlm.turboquant import turboquant_enabled
        if turboquant_enabled(kv_bits, kv_quant_scheme):
            print(f"TurboQuant enabled: {kv_bits}-bit KV cache compression (4.6x smaller)", flush=True)
        else:
            kv_quant_scheme = "uniform"
            print(f"Uniform KV quantization: {kv_bits}-bit", flush=True)

    if not args.no_prompt_cache:
        from mlx_vlm.generate import PromptCacheState
        prompt_cache_state = PromptCacheState()
        print("Prompt cache enabled: system prompt KV will be reused across turns", flush=True)

    load_model(args.model, adapter_path=args.adapter_path)

    class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
        allow_reuse_address = True
        allow_reuse_port = True
        daemon_threads = True

    server = ThreadedHTTPServer((args.host, args.port), ChatHandler)
    kv_info = f", KV={kv_bits}b TurboQuant" if kv_bits else ""
    cache_info = ", prompt-cache=on" if prompt_cache_state else ""
    adapter_info = f", adapter={args.adapter_path}" if args.adapter_path else ""
    print(f"\nServing on http://{args.host}:{args.port}/v1/chat/completions")
    print(f"Model: {args.model} ({model_id}{kv_info}{cache_info}{adapter_info})")
    print(f"Health: http://{args.host}:{args.port}/health\n", flush=True)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.shutdown()


if __name__ == "__main__":
    main()
