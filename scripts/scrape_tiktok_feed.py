#!/usr/bin/env python3
"""
Scrape TikTok For You feed headlessly using cookies from the local Chrome/PWA.
No visible browser window — runs completely in the background.

Usage:
    python scripts/scrape_tiktok_feed.py [--max-videos 30]
"""

import argparse
import json
import sys
from datetime import datetime
from pathlib import Path

try:
    from playwright.sync_api import sync_playwright, TimeoutError as PwTimeout
except ImportError:
    print("Error: run `~/.human/scraper-venv/bin/pip install playwright`")
    sys.exit(1)

INGEST_DIR = Path.home() / ".human" / "feeds" / "ingest"
STATE_FILE = Path.home() / ".human" / "browser_state" / "tiktok.json"


def scrape_tiktok(max_videos: int = 30) -> list[dict]:
    videos = []

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        context = browser.new_context(
            storage_state=str(STATE_FILE) if STATE_FILE.exists() else None,
            viewport={"width": 1280, "height": 900},
            user_agent="Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36",
        )
        page = context.new_page()

        try:
            page.goto("https://www.tiktok.com/foryou", wait_until="domcontentloaded", timeout=30000)
        except PwTimeout:
            pass

        page.wait_for_timeout(5000)
        seen = set()

        for _ in range(max_videos + 10):
            if len(videos) >= max_videos:
                break
            for el in page.query_selector_all(
                '[data-e2e="browse-video-desc"], '
                '[data-e2e="video-desc"], '
                'span.tiktok-j2a19r-SpanText'
            ):
                text = el.inner_text().strip()
                if text and len(text) > 10 and text not in seen:
                    seen.add(text)
                    link = el.evaluate(
                        "el => el.closest('[data-e2e=\"recommend-list-item-container\"]')?.querySelector('a')?.href || ''"
                    )
                    videos.append({
                        "source": "tiktok", "content_type": "video_caption",
                        "content": text[:2000], "url": link or "",
                    })
            page.evaluate("window.scrollBy(0, window.innerHeight)")
            page.wait_for_timeout(2000)

        STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
        context.storage_state(path=str(STATE_FILE))
        browser.close()

    return videos[:max_videos]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--max-videos", type=int, default=30)
    args = parser.parse_args()

    print(f"Scraping up to {args.max_videos} videos (headless, no visible window)...")
    videos = scrape_tiktok(args.max_videos)
    print(f"Scraped {len(videos)} video descriptions")

    if videos:
        INGEST_DIR.mkdir(parents=True, exist_ok=True)
        out = INGEST_DIR / f"tiktok_{datetime.now().strftime('%Y%m%d')}.jsonl"
        with open(out, "w") as f:
            for v in videos:
                f.write(json.dumps(v) + "\n")
        print(f"Wrote {len(videos)} items to {out}")


if __name__ == "__main__":
    main()
