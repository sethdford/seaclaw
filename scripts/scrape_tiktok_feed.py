#!/usr/bin/env python3
"""
Scrape TikTok For You feed using Playwright with Chrome cookies.
Extracts video captions/descriptions.
Outputs JSONL to ~/.human/feeds/ingest/tiktok_YYYYMMDD.jsonl.

Cookies are extracted from the local Chrome/PWA by extract_chrome_cookies.py.

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
    print("Error: playwright not installed. Run: pip install playwright && playwright install chromium")
    sys.exit(1)


INGEST_DIR = Path.home() / ".human" / "feeds" / "ingest"
STATE_FILE = Path.home() / ".human" / "browser_state" / "tiktok.json"
TIKTOK_URL = "https://www.tiktok.com/foryou"


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
            page.goto(TIKTOK_URL, wait_until="domcontentloaded", timeout=30000)
        except PwTimeout:
            print("Warning: page load timed out, continuing anyway...")

        page.wait_for_timeout(5000)

        seen_texts = set()
        scroll_attempts = 0
        max_scrolls = max_videos + 10

        while len(videos) < max_videos and scroll_attempts < max_scrolls:
            desc_elements = page.query_selector_all(
                '[data-e2e="browse-video-desc"], '
                '[data-e2e="video-desc"], '
                'span.tiktok-j2a19r-SpanText'
            )

            for el in desc_elements:
                text = el.inner_text().strip()
                if text and len(text) > 10 and text not in seen_texts:
                    seen_texts.add(text)
                    link = el.evaluate(
                        "el => el.closest('[data-e2e=\"recommend-list-item-container\"]')?.querySelector('a')?.href || ''"
                    )
                    videos.append({
                        "source": "tiktok",
                        "content_type": "video_caption",
                        "content": text[:2000],
                        "url": link or "",
                    })

                if len(videos) >= max_videos:
                    break

            page.evaluate("window.scrollBy(0, window.innerHeight)")
            page.wait_for_timeout(2000)
            scroll_attempts += 1

        STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
        context.storage_state(path=str(STATE_FILE))
        browser.close()

    return videos


def main():
    parser = argparse.ArgumentParser(description="Scrape TikTok For You feed")
    parser.add_argument("--max-videos", type=int, default=30,
                        help="Maximum videos to scrape (default: 30)")
    args = parser.parse_args()

    print(f"Scraping up to {args.max_videos} videos from TikTok...")
    videos = scrape_tiktok(args.max_videos)
    print(f"Scraped {len(videos)} video descriptions")

    if videos:
        INGEST_DIR.mkdir(parents=True, exist_ok=True)
        outfile = INGEST_DIR / f"tiktok_{datetime.now().strftime('%Y%m%d')}.jsonl"
        with open(outfile, "w") as f:
            for v in videos:
                f.write(json.dumps(v) + "\n")
        print(f"Wrote {len(videos)} items to {outfile}")
    else:
        print("No videos scraped — check cookies or login state")


if __name__ == "__main__":
    main()
