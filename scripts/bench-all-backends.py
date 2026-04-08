#!/usr/bin/env python3
"""
Head-to-head benchmark of all inference backends for Gemma 4 on Apple Silicon.

Tests each backend against the same voice prompt workload and produces a
comparison table with TTFT, tok/s, RTF, and real-time readiness verdict.

Backends tested:
  1. MLX Server (mlx_lm)       — Python + MLX, fast text path
  2. Ollama                     — Go + llama.cpp Metal
  3. llama.cpp server           — C++ + Metal, fused kernels
  4. vLLM Metal                 — Python + paged attention
  5. ANE+GPU bridge             — Dual-compute speculative decoding

Usage:
    python3 scripts/bench-all-backends.py
    python3 scripts/bench-all-backends.py --backends mlx,ollama,llamacpp
    python3 scripts/bench-all-backends.py --rounds 20 --json

Each backend should be started on its own port before running this script.
Default ports:
    MLX Server:     8741
    Ollama:         11434
    llama.cpp:      8742
    vLLM:           8743
    ANE+GPU:        8744
"""
from __future__ import annotations

import argparse
import json
import statistics
import sys
import time
import urllib.request

# ── Backend registry ─────────────────────────────────────────────
BACKENDS = {
    "mlx": {
        "name": "MLX Server (mlx_lm)",
        "port": 8741,
        "model": None,  # auto-detect from /health
        "description": "Python + MLX, fast text path (no vision overhead)",
        "start_cmd": "python3 scripts/mlx-server.py --realtime",
    },
    "ollama": {
        "name": "Ollama (Go + llama.cpp)",
        "port": 11434,
        "model": "gemma3:4b",  # explicit: Ollama lists multiple, pick the voice-sized one
        "description": "Go HTTP server + llama.cpp Metal backend, zero GIL",
        "start_cmd": "./scripts/ollama-serve.sh",
    },
    "llamacpp": {
        "name": "llama.cpp Metal",
        "port": 8742,
        "model": None,  # auto-detect from /v1/models
        "description": "C++ server, fused Metal kernels, flash attention v2",
        "start_cmd": "./scripts/llamacpp-serve.sh --port 8742",
    },
    "vllm": {
        "name": "vLLM Metal",
        "port": 8743,
        "model": None,
        "description": "Paged attention, continuous batching, chunked prefill",
        "start_cmd": "./scripts/vllm-metal-serve.sh --port 8743",
    },
    "ane-gpu": {
        "name": "ANE+GPU Bridge",
        "port": 8744,
        "model": None,
        "description": "Dual-compute: E2B on ANE, E4B on GPU, speculative decoding",
        "start_cmd": "python3 scripts/ane-gpu-bridge.py --port 8744",
    },
}

VOICE_PROMPTS = [
    "hey what's up",
    "what are you working on today?",
    "tell me something interesting",
    "how's the weather looking?",
    "what should I have for dinner?",
    "can you remind me about that thing?",
    "I'm feeling kind of stressed",
    "what do you think about that?",
    "give me a quick summary",
    "thanks, that's helpful",
    "lol that's funny",
    "yo",
    "what time is it",
    "can you help me with something quick?",
    "I've been thinking about switching jobs",
]

SYSTEM_PROMPT = (
    "You are a conversational voice assistant. Keep responses concise and natural, "
    "as if speaking aloud. 1-3 sentences max."
)


def check_backend(endpoint: str) -> dict | None:
    """Check if a backend is alive and return its health info.

    Tries multiple health endpoints to support different backends:
      - /health (MLX server, ANE bridge)
      - /api/version (Ollama)
      - /health (llama.cpp — returns HTML, but 200 = alive)
      - / (Ollama root)
    """
    # Try /health first (MLX, llama.cpp, vLLM)
    for health_path in ["/health", "/api/version", "/"]:
        try:
            req = urllib.request.Request(f"{endpoint}{health_path}", method="GET")
            resp = urllib.request.urlopen(req, timeout=3)
            body = resp.read()
            try:
                data = json.loads(body)
                return data
            except (json.JSONDecodeError, ValueError):
                return {"status": "ok", "info": body.decode("utf-8", errors="replace").strip()[:100]}
        except Exception:
            continue
    return None


def discover_model(endpoint: str) -> str | None:
    """Auto-detect the model name from a backend's /v1/models endpoint."""
    try:
        req = urllib.request.Request(f"{endpoint}/v1/models", method="GET")
        resp = urllib.request.urlopen(req, timeout=3)
        data = json.loads(resp.read())
        models = data.get("data", [])
        if models:
            return models[0].get("id")
    except Exception:
        pass
    return None


def bench_single_request(endpoint: str, prompt: str, model: str = None) -> dict:
    """Send a single streaming request and measure timing."""
    payload = {
        "messages": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": prompt},
        ],
        "stream": True,
        "max_tokens": 128,
        "temperature": 0.7,
    }
    if model:
        payload["model"] = model

    req = urllib.request.Request(
        f"{endpoint}/v1/chat/completions",
        data=json.dumps(payload).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )

    t0 = time.perf_counter()
    first_token_time = None
    total_tokens = 0
    full_text = []

    try:
        resp = urllib.request.urlopen(req, timeout=30)
        for line in resp:
            line = line.decode("utf-8", errors="replace").strip()
            if not line.startswith("data: "):
                continue
            data = line[6:]
            if data == "[DONE]":
                break
            try:
                chunk = json.loads(data)
                choices = chunk.get("choices", [])
                if choices:
                    delta = choices[0].get("delta", {})
                    content = delta.get("content", "")
                    if content:
                        if first_token_time is None:
                            first_token_time = time.perf_counter()
                        total_tokens += 1
                        full_text.append(content)
            except json.JSONDecodeError:
                continue
    except Exception as e:
        return {"error": str(e)}

    t_end = time.perf_counter()
    ttft_ms = (first_token_time - t0) * 1000 if first_token_time else 0
    total_ms = (t_end - t0) * 1000
    gen_time = (t_end - first_token_time) if first_token_time else 0
    tps = total_tokens / gen_time if gen_time > 0 else 0

    text = "".join(full_text)
    words = len(text.split())
    speech_time = words / 2.5
    rtf = gen_time / speech_time if speech_time > 0 else 0

    return {
        "ttft_ms": round(ttft_ms, 1),
        "total_ms": round(total_ms, 1),
        "tokens": total_tokens,
        "tps": round(tps, 1),
        "rtf": round(rtf, 3),
    }


def bench_backend(name: str, config: dict, rounds: int, warmup: int) -> dict:
    """Run full benchmark suite against one backend."""
    endpoint = f"http://127.0.0.1:{config['port']}"
    health = check_backend(endpoint)
    if health is None:
        return {"name": name, "status": "offline", "endpoint": endpoint}

    # Use configured model first, then health endpoint, then /v1/models discovery
    model_name = config.get("model") or health.get("model", None) or discover_model(endpoint)

    # Warmup
    for i in range(warmup):
        bench_single_request(endpoint, VOICE_PROMPTS[i % len(VOICE_PROMPTS)], model_name)

    # Benchmark
    results = []
    for i in range(rounds):
        prompt = VOICE_PROMPTS[i % len(VOICE_PROMPTS)]
        r = bench_single_request(endpoint, prompt, model_name)
        if "error" not in r:
            results.append(r)

    if not results:
        return {"name": name, "status": "error", "endpoint": endpoint}

    ttfts = [r["ttft_ms"] for r in results]
    tpss = [r["tps"] for r in results]
    rtfs = [r["rtf"] for r in results]

    return {
        "name": name,
        "display_name": config["name"],
        "status": "ok",
        "endpoint": endpoint,
        "health": health,
        "rounds": len(results),
        "ttft_p50": round(statistics.median(ttfts), 1),
        "ttft_p95": round(sorted(ttfts)[int(len(ttfts) * 0.95)] if len(ttfts) > 1 else ttfts[0], 1),
        "tps_p50": round(statistics.median(tpss), 1),
        "tps_p95": round(sorted(tpss)[int(len(tpss) * 0.95)] if len(tpss) > 1 else tpss[0], 1),
        "tps_mean": round(statistics.mean(tpss), 1),
        "rtf_p50": round(statistics.median(rtfs), 3),
        "realtime": statistics.median(ttfts) < 200 and statistics.median(tpss) > 50,
    }


def print_comparison(results: list[dict]):
    """Print a formatted comparison table."""
    print(f"\n{'='*90}")
    print(f"  HEAD-TO-HEAD BENCHMARK RESULTS")
    print(f"{'='*90}")

    # Header
    print(f"\n  {'Backend':<28s} {'TTFT P50':>10s} {'TTFT P95':>10s} "
          f"{'TPS P50':>9s} {'TPS Mean':>9s} {'RTF P50':>9s} {'Verdict':>10s}")
    print(f"  {'─'*26}  {'─'*8}  {'─'*8}  {'─'*7}  {'─'*7}  {'─'*7}  {'─'*8}")

    # Sort by TPS (fastest first)
    online = [r for r in results if r["status"] == "ok"]
    offline = [r for r in results if r["status"] != "ok"]
    online.sort(key=lambda r: r["tps_p50"], reverse=True)

    for r in online:
        verdict = "REALTIME" if r["realtime"] else "not yet"
        tps_bar = ">" * min(int(r["tps_p50"] / 10), 10)
        print(f"  {r['display_name']:<28s} {r['ttft_p50']:>8.0f}ms {r['ttft_p95']:>8.0f}ms "
              f"{r['tps_p50']:>7.1f}  {r['tps_mean']:>7.1f}  {r['rtf_p50']:>7.3f}  "
              f"{verdict:>8s}  {tps_bar}")

    for r in offline:
        display = BACKENDS.get(r["name"], {}).get("name", r["name"])
        print(f"  {display:<28s} {'—':>10s} {'—':>10s} {'—':>9s} {'—':>9s} {'—':>9s} "
              f"{'OFFLINE':>10s}")

    # Summary
    print(f"\n  {'─'*88}")
    if online:
        fastest = online[0]
        print(f"  Fastest backend: {fastest['display_name']} at {fastest['tps_p50']} tok/s")
        realtime = [r for r in online if r["realtime"]]
        if realtime:
            print(f"  Real-time ready: {', '.join(r['display_name'] for r in realtime)}")
        else:
            print(f"  Real-time ready: NONE (target: TTFT <200ms + TPS >50)")

    if offline:
        print(f"\n  Offline backends (start them to include in benchmark):")
        for r in offline:
            cfg = BACKENDS.get(r["name"], {})
            print(f"    {cfg.get('name', r['name'])}: {cfg.get('start_cmd', '?')}")

    print(f"{'='*90}\n")


def main():
    parser = argparse.ArgumentParser(
        description="Head-to-head benchmark of all inference backends",
    )
    parser.add_argument("--backends", default=None,
                        help="Comma-separated list of backends to test (default: all)")
    parser.add_argument("--rounds", type=int, default=10,
                        help="Benchmark rounds per backend (default: 10)")
    parser.add_argument("--warmup", type=int, default=2,
                        help="Warmup rounds per backend (default: 2)")
    parser.add_argument("--json", action="store_true",
                        help="Output raw JSON results")
    args = parser.parse_args()

    if args.backends:
        backend_names = [b.strip() for b in args.backends.split(",")]
    else:
        backend_names = list(BACKENDS.keys())

    print(f"\n  Scanning backends...\n")
    for name in backend_names:
        cfg = BACKENDS.get(name)
        if not cfg:
            print(f"  Unknown backend: {name}")
            continue
        endpoint = f"http://127.0.0.1:{cfg['port']}"
        health = check_backend(endpoint)
        status = "ONLINE" if health else "offline"
        if health:
            model = cfg.get("model") or health.get("model", None) or discover_model(endpoint) or "?"
            engine = health.get("engine", health.get("version", "?"))
        else:
            model = "—"
            engine = "—"
        print(f"  {cfg['name']:<28s} :{cfg['port']}  {status:<8s} model={model} engine={engine}")

    print(f"\n  Running {args.rounds} rounds (+{args.warmup} warmup) per backend...")

    results = []
    for name in backend_names:
        cfg = BACKENDS.get(name)
        if not cfg:
            continue
        print(f"\n  --- {cfg['name']} ---")
        r = bench_backend(name, cfg, args.rounds, args.warmup)
        results.append(r)

        if r["status"] == "ok":
            print(f"  TTFT P50={r['ttft_p50']}ms  TPS P50={r['tps_p50']}  RTF={r['rtf_p50']}")
        else:
            print(f"  {r['status'].upper()}")

    if args.json:
        print(json.dumps(results, indent=2))
    else:
        print_comparison(results)


if __name__ == "__main__":
    main()
