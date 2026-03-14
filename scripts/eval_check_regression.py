#!/usr/bin/env python3
"""Check eval results against regression thresholds. Exit 0 on pass, 1 on regression."""

import argparse
import json
import sys


def main() -> None:
    parser = argparse.ArgumentParser(description="Check eval regression thresholds")
    parser.add_argument("--results", default="eval_results.json", help="Path to eval_results.json")
    parser.add_argument("--min-relevance", type=float, default=6.0, help="Minimum avg relevance")
    parser.add_argument("--min-pass-rate", type=float, default=0.8, help="Minimum pass rate")
    args = parser.parse_args()

    with open(args.results, encoding="utf-8") as f:
        data = json.load(f)

    summary = data.get("summary", {})
    avg_relevance = summary.get("avg_relevance", 0)
    pass_rate = summary.get("pass_rate", 0)
    total = summary.get("total", 0)

    print(f"Eval summary: {total} cases, avg_relevance={avg_relevance:.2f}, pass_rate={pass_rate:.2f}")
    print(f"Thresholds: avg_relevance>={args.min_relevance}, pass_rate>={args.min_pass_rate}")

    ok = avg_relevance >= args.min_relevance and pass_rate >= args.min_pass_rate
    if not ok:
        print("REGRESSION: thresholds not met", file=sys.stderr)
        sys.exit(1)
    print("PASS: thresholds met")
    sys.exit(0)


if __name__ == "__main__":
    main()
