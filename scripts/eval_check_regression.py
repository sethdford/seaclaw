#!/usr/bin/env python3
"""Check eval results against regression thresholds. Exit 0 on pass, 1 on regression.

Supports both absolute thresholds and historical regression detection (>5% drop
from the rolling average of recent runs stored in eval_history.json).
"""

import argparse
import json
import os
import sys

HISTORY_FILE = "eval_history.json"
MAX_HISTORY = 20
DEFAULT_REGRESSION_THRESHOLD = 0.05


def load_history(path: str) -> list[dict]:
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def save_history(path: str, history: list[dict]) -> None:
    with open(path, "w", encoding="utf-8") as f:
        json.dump(history[-MAX_HISTORY:], f, indent=2)


def main() -> None:
    parser = argparse.ArgumentParser(description="Check eval regression thresholds")
    parser.add_argument("--results", default="eval_results.json", help="Path to eval_results.json")
    parser.add_argument("--history", default=HISTORY_FILE, help="Path to eval history file")
    parser.add_argument("--min-relevance", type=float, default=6.0, help="Minimum avg relevance")
    parser.add_argument("--min-pass-rate", type=float, default=0.8, help="Minimum pass rate")
    parser.add_argument(
        "--regression-threshold",
        type=float,
        default=DEFAULT_REGRESSION_THRESHOLD,
        help="Max allowed drop from historical average (default 0.05 = 5%%)",
    )
    args = parser.parse_args()

    with open(args.results, encoding="utf-8") as f:
        data = json.load(f)

    summary = data.get("summary", {})
    avg_relevance = summary.get("avg_relevance", 0)
    pass_rate = summary.get("pass_rate", 0)
    total = summary.get("total", 0)

    print(f"Eval summary: {total} cases, avg_relevance={avg_relevance:.2f}, pass_rate={pass_rate:.2f}")
    print(f"Absolute thresholds: avg_relevance>={args.min_relevance}, pass_rate>={args.min_pass_rate}")

    failed = False

    if avg_relevance < args.min_relevance or pass_rate < args.min_pass_rate:
        print("FAIL: absolute thresholds not met", file=sys.stderr)
        failed = True

    history = load_history(args.history)
    if history:
        recent = history[-10:]
        baseline_avg = sum(r["pass_rate"] for r in recent) / len(recent)
        delta = pass_rate - baseline_avg
        print(
            f"Historical: baseline_avg={baseline_avg:.4f} (from {len(recent)} runs), "
            f"current={pass_rate:.4f}, delta={delta:+.4f}"
        )
        if delta < -args.regression_threshold:
            print(
                f"REGRESSION: pass_rate dropped {abs(delta):.4f} from baseline "
                f"(threshold={args.regression_threshold})",
                file=sys.stderr,
            )
            failed = True
        else:
            print("Historical regression check: PASS")
    else:
        print("No historical data — skipping regression comparison")

    history.append({"pass_rate": pass_rate, "avg_relevance": avg_relevance, "total": total})
    save_history(args.history, history)

    if failed:
        sys.exit(1)
    print("PASS: all checks passed")
    sys.exit(0)


if __name__ == "__main__":
    main()
