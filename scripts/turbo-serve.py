#!/usr/bin/env python3
"""TurboQuant+ server wrapper for h-uman.

Monkey-patches mlx_lm to use TurboKVCache (compressed KV cache) before
launching the standard mlx_lm.server. Reduces KV cache memory by ~50%
with near-baseline quality (+0.51% PPL for asymmetric K=FP16, V=turbo4).

Usage:
    python3 scripts/turbo-serve.py --model <model> --port <port> [--adapter-path <path>]
    python3 scripts/turbo-serve.py --symmetric  # compress both K and V to turbo4

Environment variables:
    TURBO_MODE          "asymmetric" (default, K=FP16/V=turbo4) or "symmetric"
    TURBO_BITS          V compression bits (default: 4)
    TURBO_K_COMPRESS_THRESHOLD  Adaptive K compression threshold (default: 0)
"""

import os
import sys


def patch_cache_factory():
    """Replace KVCache with TurboKVCache in mlx_lm's cache factory."""
    os.environ["TURBO_ASYNC_ENCODE"] = "0"

    from mlx.nn.layers.turbo_kv_cache import TurboKVCache, patch_mlx_lm
    import mlx_lm.models.cache as cache_mod

    mode = os.environ.get("TURBO_MODE", "asymmetric")
    bits = int(os.environ.get("TURBO_BITS", "4"))

    if mode == "symmetric":
        key_bits = bits
    else:
        key_bits = 0  # FP16 keys, turbo4 values

    _original_make_prompt_cache = cache_mod.make_prompt_cache

    def turbo_make_prompt_cache(model, max_kv_size=None):
        if hasattr(model, "make_cache"):
            cache = model.make_cache()
        else:
            num_layers = len(model.layers)
            cache = [
                TurboKVCache(bits=bits, key_bits=key_bits)
                for _ in range(num_layers)
            ]

        patch_mlx_lm(cache)
        return cache

    cache_mod.make_prompt_cache = turbo_make_prompt_cache

    kb = "FP16" if key_bits == 0 else f"turbo{key_bits}"
    print(f"[turbo-serve] KV cache: K={kb}, V=turbo{bits} ({mode} mode)")


if __name__ == "__main__":
    patch_cache_factory()

    from mlx_lm.server import main
    main()
