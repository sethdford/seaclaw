#!/usr/bin/env python3
"""
Real-time voice benchmark — measure TTFT, tok/s, and RTF for the MLX server.

Simulates the voice pipeline's request pattern: short conversational prompts
with streaming responses, measuring the metrics that matter for real-time voice.

Usage:
  python3 scripts/voice-bench.py
  python3 scripts/voice-bench.py --endpoint http://127.0.0.1:8741 --rounds 20
  python3 scripts/voice-bench.py --compare  # compare E4B vs 31B vs speculative

Metrics:
  TTFT  — Time to First Token (ms). Target: <200ms for "feels instant"
  TPS   — Tokens Per Second. Target: >50 tok/s for real-time voice
  RTF   — Real-Time Factor for text generation. <1.0 means faster than speech
  E2E   — End-to-end response time (ms). Full generation time.
"""
from __future__ import annotations

import argparse
import json
import statistics
import sys
import time
import urllib.request
from pathlib import Path

DEFAULT_ENDPOINT = "http://127.0.0.1:8741"

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
    "how do you feel about that?",
    "tell me a joke",
    "what's the best way to learn rust?",
    "I need to focus, any tips?",
    "goodnight",
]

SYSTEM_PROMPT = (
    "You are a conversational voice assistant. Keep responses concise and natural, "
    "as if speaking aloud. 1-3 sentences max unless the topic needs depth."
)


def stream_request(endpoint: str, prompt: str, system: str = SYSTEM_PROMPT,
                   max_tokens: int = 128, temperature: float = 0.7,
                   model: str | None = None) -> dict:
    """Send a streaming request and measure timing."""
    body = {
        "messages": [
            {"role": "system", "content": system},
            {"role": "user", "content": prompt},
        ],
        "stream": True,
        "max_tokens": max_tokens,
        "temperature": temperature,
    }
    if model:
        body["model"] = model
    payload = json.dumps(body).encode()

    req = urllib.request.Request(
        f"{endpoint}/v1/chat/completions",
        data=payload,
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
        return {"error": str(e), "prompt": prompt}

    t_end = time.perf_counter()
    ttft_ms = (first_token_time - t0) * 1000 if first_token_time else 0
    total_ms = (t_end - t0) * 1000
    gen_time = (t_end - first_token_time) if first_token_time else 0
    tps = total_tokens / gen_time if gen_time > 0 else 0

    text = "".join(full_text)
    words = len(text.split())
    speech_duration_s = words / 2.5
    rtf = gen_time / speech_duration_s if speech_duration_s > 0 else 0

    return {
        "prompt": prompt,
        "ttft_ms": round(ttft_ms, 1),
        "total_ms": round(total_ms, 1),
        "tokens": total_tokens,
        "tps": round(tps, 1),
        "rtf": round(rtf, 3),
        "words": words,
        "text_preview": text[:80],
    }


def run_benchmark(endpoint: str, rounds: int, warmup: int = 2, model_override: str = None) -> list[dict]:
    """Run the benchmark suite."""
    health = None
    for health_path in ["/health", "/api/version", "/"]:
        try:
            req = urllib.request.Request(f"{endpoint}{health_path}", method="GET")
            resp = urllib.request.urlopen(req, timeout=5)
            body = resp.read()
            try:
                health = json.loads(body)
            except json.JSONDecodeError:
                health = {"status": "ok", "info": body.decode().strip()[:100]}
            break
        except Exception:
            continue
    if health is None:
        print(f"ERROR: Cannot reach server at {endpoint}")
        print(f"Start it with: python3 scripts/mlx-server.py --realtime")
        return []

    print(f"\n{'='*70}")
    print(f"  Real-Time Voice Benchmark")
    print(f"{'='*70}")
    print(f"  Endpoint:  {endpoint}")
    print(f"  Model:     {health.get('model', 'unknown')}")
    if health.get("speculative_decoding"):
        print(f"  Speculate: YES (draft tokens: {health.get('draft_tokens', '?')})")
    if health.get("hardware"):
        hw = health["hardware"]
        print(f"  Hardware:  {hw.get('chip', 'unknown')} ({hw.get('unified_memory_gb', '?')} GB)")
        if hw.get("has_tensor_ops"):
            print(f"  TensorOps: ENABLED")
    if health.get("adapter"):
        print(f"  Adapter:   {health['adapter']}")
    print(f"  Rounds:    {rounds} (+{warmup} warmup)")
    print(f"{'='*70}\n")

    # Discover model name for backends that require it (e.g. Ollama)
    bench_model = model_override
    if bench_model is None:
        try:
            req = urllib.request.Request(f"{endpoint}/v1/models", method="GET")
            resp = urllib.request.urlopen(req, timeout=5)
            models_data = json.loads(resp.read())
            model_list = models_data.get("data", [])
            if model_list:
                bench_model = model_list[0].get("id")
        except Exception:
            pass

    if warmup > 0:
        print(f"  Warmup ({warmup} rounds)...", end="", flush=True)
        for i in range(warmup):
            stream_request(endpoint, VOICE_PROMPTS[i % len(VOICE_PROMPTS)], model=bench_model)
            print(".", end="", flush=True)
        print(" done\n")

    results = []
    for i in range(rounds):
        prompt = VOICE_PROMPTS[i % len(VOICE_PROMPTS)]
        r = stream_request(endpoint, prompt, model=bench_model)
        results.append(r)

        status = "OK" if "error" not in r else f"ERR: {r['error']}"
        if "error" not in r:
            print(f"  [{i+1:3d}/{rounds}] TTFT={r['ttft_ms']:6.0f}ms  "
                  f"TPS={r['tps']:5.1f}  RTF={r['rtf']:.3f}  "
                  f"tokens={r['tokens']:3d}  | {r['text_preview'][:40]}...")
        else:
            print(f"  [{i+1:3d}/{rounds}] {status}")

    return results


def print_summary(results: list[dict]):
    """Print statistical summary of benchmark results."""
    valid = [r for r in results if "error" not in r]
    if not valid:
        print("\n  No valid results to summarize.")
        return

    ttfts = [r["ttft_ms"] for r in valid]
    tpss = [r["tps"] for r in valid]
    rtfs = [r["rtf"] for r in valid]
    totals = [r["total_ms"] for r in valid]

    print(f"\n{'='*70}")
    print(f"  RESULTS ({len(valid)} valid / {len(results)} total)")
    print(f"{'='*70}")

    def stats_line(name, values, unit, target=None):
        p50 = statistics.median(values)
        p95 = sorted(values)[int(len(values) * 0.95)] if len(values) > 1 else values[0]
        p99 = sorted(values)[int(len(values) * 0.99)] if len(values) > 1 else values[0]
        mean = statistics.mean(values)
        target_str = ""
        if target:
            met = "PASS" if p50 <= target else "MISS"
            target_str = f"  [target: {target}{unit} → {met}]"
        print(f"  {name:10s}  P50={p50:7.1f}{unit}  P95={p95:7.1f}{unit}  "
              f"P99={p99:7.1f}{unit}  mean={mean:7.1f}{unit}{target_str}")

    stats_line("TTFT", ttfts, "ms", target=200)
    stats_line("TPS", tpss, " t/s", target=50)
    stats_line("RTF", rtfs, "x")
    stats_line("E2E", totals, "ms")

    p50_ttft = statistics.median(ttfts)
    p50_tps = statistics.median(tpss)

    print(f"\n  Voice readiness:")
    if p50_ttft < 200 and p50_tps > 50:
        print(f"    REAL-TIME READY — TTFT and TPS both meet voice targets")
    elif p50_ttft < 200:
        print(f"    TTFT is good (<200ms) but TPS needs improvement (target: >50)")
    elif p50_tps > 50:
        print(f"    TPS is good (>50 tok/s) but TTFT needs improvement (target: <200ms)")
    else:
        print(f"    NOT YET REAL-TIME — both TTFT and TPS need improvement")
        print(f"    Try: --realtime flag, E4B model, or speculative decoding")

    print(f"{'='*70}\n")


def main():
    parser = argparse.ArgumentParser(
        description="Benchmark MLX server for real-time voice latency",
    )
    parser.add_argument("--endpoint", default=DEFAULT_ENDPOINT,
                        help=f"MLX server endpoint (default: {DEFAULT_ENDPOINT})")
    parser.add_argument("--rounds", type=int, default=10,
                        help="Number of benchmark rounds (default: 10)")
    parser.add_argument("--warmup", type=int, default=2,
                        help="Number of warmup rounds (default: 2)")
    parser.add_argument("--model", default=None,
                        help="Model name to pass in request (required for Ollama)")
    parser.add_argument("--json", action="store_true",
                        help="Output results as JSON")
    parser.add_argument("--compare", action="store_true",
                        help="Compare all backends (runs bench-all-backends.py)")
    args = parser.parse_args()

    if args.compare:
        import subprocess
        script = str(Path(__file__).parent / "bench-all-backends.py")
        extra = ["--rounds", str(args.rounds), "--warmup", str(args.warmup)]
        if args.json:
            extra.append("--json")
        sys.exit(subprocess.run([sys.executable, script] + extra).returncode)

    results = run_benchmark(args.endpoint, args.rounds, args.warmup, args.model)
    if not results:
        sys.exit(1)

    if args.json:
        print(json.dumps(results, indent=2))
    else:
        print_summary(results)


if __name__ == "__main__":
    main()
