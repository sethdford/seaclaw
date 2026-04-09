#!/usr/bin/env python3
"""
MLX inference server — delegates to gemma-realtime's mlx-server.py.

This shim finds and runs the gemma-realtime version which has:
  - TurboQuant+ KV cache compression (3.8-6.4x)
  - Speculative decoding (E2B draft → E4B target)
  - ~/.human/config.json auto-detection
  - PLE-safe model validation
  - Apple Silicon hardware detection

Falls back to a minimal built-in server if gemma-realtime is not installed.
"""

import os
import sys
import runpy

GEMMA_RT_PATHS = [
    os.path.expanduser("~/Documents/gemma-realtime-1/scripts/mlx-server.py"),
    os.path.expanduser("~/Documents/gemma-realtime/scripts/mlx-server.py"),
    os.path.expanduser("~/gemma-realtime/scripts/mlx-server.py"),
]


def find_gemma_realtime():
    for p in GEMMA_RT_PATHS:
        if os.path.isfile(p):
            return p
    return None


def main():
    server_path = find_gemma_realtime()
    if server_path:
        sys.argv[0] = server_path
        parent = os.path.dirname(server_path)
        if parent not in sys.path:
            sys.path.insert(0, parent)
        spec_name = os.path.splitext(os.path.basename(server_path))[0]

        import importlib.util
        spec = importlib.util.spec_from_file_location("__main__", server_path)
        mod = importlib.util.module_from_spec(spec)
        sys.modules["__main__"] = mod
        spec.loader.exec_module(mod)
        return

    print("gemma-realtime not found, falling back to mlx_lm.server", flush=True)
    print("Install: git clone https://github.com/sethdford/gemma-realtime ~/Documents/gemma-realtime-1", flush=True)
    from mlx_lm.server import main as mlx_main
    mlx_main()


if __name__ == "__main__":
    main()
