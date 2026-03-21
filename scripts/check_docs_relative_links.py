#!/usr/bin/env python3
"""
Scan docs/**/*.md for relative Markdown links and verify targets exist.
Skips http(s)://, mailto:, javascript:, and pure #anchors.
Exit 1 if any target is missing.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

# [text](url) or [text](url "title")
LINK_RE = re.compile(r"\[[^\]]*\]\(([^)\s]+)(?:\s+[\"'][^\"']*[\"'])?\)")

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"


def main() -> int:
    if not DOCS.is_dir():
        print("ERROR: docs/ not found", file=sys.stderr)
        return 1

    broken: list[tuple[str, str, str]] = []
    md_files = sorted(DOCS.rglob("*.md"))

    for md_path in md_files:
        try:
            text = md_path.read_text(encoding="utf-8")
        except OSError as e:
            print(f"WARN: could not read {md_path}: {e}", file=sys.stderr)
            continue
        parent = md_path.parent
        for m in LINK_RE.finditer(text):
            raw = m.group(1).strip()
            if not raw or raw.startswith(("#", "http://", "https://", "mailto:", "javascript:")):
                continue
            # strip angle brackets some authors use
            if raw.startswith("<") and raw.endswith(">"):
                raw = raw[1:-1]
            path_part = raw.split("#", 1)[0]
            if not path_part:
                continue
            target = (parent / path_part).resolve()
            try:
                target.relative_to(ROOT)
            except ValueError:
                broken.append((str(md_path.relative_to(ROOT)), raw, "escapes repo root"))
                continue
            if not target.exists():
                broken.append((str(md_path.relative_to(ROOT)), raw, "missing"))

    if broken:
        print("Broken or invalid relative links in docs/:\n")
        for src, href, reason in broken:
            print(f"  {src}")
            print(f"    -> {href} ({reason})")
        print(f"\nTotal: {len(broken)}")
        return 1

    print(f"OK: relative links resolved for {len(md_files)} Markdown file(s) under docs/.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
