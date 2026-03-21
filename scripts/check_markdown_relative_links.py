#!/usr/bin/env python3
"""
Scan Markdown under configured repo roots for relative links and verify targets exist.
Skips http(s)://, mailto:, javascript:, and pure #anchors.

Environment:
  MARKDOWN_LINK_ROOTS — space-separated top-level dirs under repo root (default below).
  MARKDOWN_LINK_SCAN_ALL=1 — scan every *.md under repo except known junk dirs.
"""
from __future__ import annotations

import os
import re
import sys
from pathlib import Path

# [text](url) or [text](url "title")
LINK_RE = re.compile(r"\[[^\]]*\]\(([^)\s]+)(?:\s+[\"'][^\"']*[\"'])?\)")

ROOT = Path(__file__).resolve().parent.parent

DEFAULT_ROOTS = ("docs", "human-skills", "skill-registry")

JUNK_DIR_NAMES = frozenset(
    {
        ".git",
        "node_modules",
        "build",
        "build-arm64",
        "build-check",
        "build-minimal",
        "dist",
        ".cache",
        "__pycache__",
        ".pytest_cache",
        "DerivedData",
    }
)


def junk_path(path: Path) -> bool:
    try:
        rel = path.relative_to(ROOT)
    except ValueError:
        return True
    parts = rel.parts
    for i, part in enumerate(parts):
        if part in JUNK_DIR_NAMES:
            return True
        # Claude plugin vendor trees (huge; not maintained as repo docs)
        if i >= 2 and parts[i - 2] == "plugins" and parts[i - 1] == "cache":
            return True
    return False


def collect_md_files() -> list[Path]:
    scan_all = os.environ.get("MARKDOWN_LINK_SCAN_ALL", "").strip() == "1"
    files: set[Path] = set()

    if scan_all:
        for p in ROOT.rglob("*.md"):
            if p.is_file() and not junk_path(p):
                files.add(p.resolve())
    else:
        roots_env = os.environ.get("MARKDOWN_LINK_ROOTS", "").strip()
        if roots_env:
            names = [x.strip() for x in roots_env.split() if x.strip()]
        else:
            names = list(DEFAULT_ROOTS)
        for name in names:
            d = ROOT / name
            if d.is_dir():
                for p in d.rglob("*.md"):
                    if p.is_file():
                        files.add(p.resolve())
        for p in ROOT.glob("*.md"):
            if p.is_file():
                files.add(p.resolve())

    return sorted(files)


def main() -> int:
    md_files = collect_md_files()
    if not md_files:
        print("ERROR: no Markdown files matched", file=sys.stderr)
        return 1

    broken: list[tuple[str, str, str]] = []

    for md_path in md_files:
        try:
            text = md_path.read_text(encoding="utf-8")
        except OSError as e:
            print(f"WARN: could not read {md_path}: {e}", file=sys.stderr)
            continue
        parent = md_path.parent
        rel_src = str(md_path.relative_to(ROOT))
        for m in LINK_RE.finditer(text):
            raw = m.group(1).strip()
            if not raw or raw.startswith(("#", "http://", "https://", "mailto:", "javascript:")):
                continue
            if raw.startswith("<") and raw.endswith(">"):
                raw = raw[1:-1]
            path_part = raw.split("#", 1)[0]
            if not path_part:
                continue
            target = (parent / path_part).resolve()
            try:
                target.relative_to(ROOT)
            except ValueError:
                broken.append((rel_src, raw, "escapes repo root"))
                continue
            if not target.exists():
                broken.append((rel_src, raw, "missing"))

    if broken:
        print("Broken or invalid relative Markdown links:\n")
        for src, href, reason in broken:
            print(f"  {src}")
            print(f"    -> {href} ({reason})")
        print(f"\nTotal: {len(broken)}")
        return 1

    roots_note = "MARKDOWN_LINK_SCAN_ALL=1" if os.environ.get("MARKDOWN_LINK_SCAN_ALL") == "1" else f"roots {DEFAULT_ROOTS} + *.md at repo root"
    print(f"OK: relative links resolved for {len(md_files)} Markdown file(s) ({roots_note}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
