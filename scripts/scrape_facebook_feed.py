#!/usr/bin/env python3
"""
Scrape Facebook news feed headlessly using cookies from the local Chrome/PWA.
No visible browser window — runs completely in the background.

Usage:
    python scripts/scrape_facebook_feed.py [--max-posts 30]
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
STATE_FILE = Path.home() / ".human" / "browser_state" / "facebook.json"


def scrape_facebook(max_posts: int = 30) -> list[dict]:
    posts = []

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        context = browser.new_context(
            storage_state=str(STATE_FILE) if STATE_FILE.exists() else None,
            viewport={"width": 1280, "height": 900},
            user_agent="Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36",
        )
        page = context.new_page()

        try:
            page.goto("https://www.facebook.com/", wait_until="domcontentloaded", timeout=30000)
        except PwTimeout:
            pass

        page.wait_for_timeout(5000)
        seen = set()

        for _ in range(max_posts // 3 + 15):
            if len(posts) >= max_posts:
                break
            for el in page.query_selector_all(
                '[data-ad-comet-preview="message"], '
                '[data-ad-preview="message"], '
                'div[dir="auto"][style*="text-align"]'
            ):
                text = el.inner_text().strip()
                if text and len(text) > 20 and text not in seen:
                    seen.add(text)
                    posts.append({
                        "source": "facebook", "content_type": "post",
                        "content": text[:2000], "url": "",
                    })
            page.evaluate("window.scrollBy(0, window.innerHeight * 1.5)")
            page.wait_for_timeout(2000)

        STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
        context.storage_state(path=str(STATE_FILE))
        browser.close()

    return posts[:max_posts]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--max-posts", type=int, default=30)
    args = parser.parse_args()

    print(f"Scraping up to {args.max_posts} posts (headless, no visible window)...")
    posts = scrape_facebook(args.max_posts)
    print(f"Scraped {len(posts)} posts")

    if posts:
        INGEST_DIR.mkdir(parents=True, exist_ok=True)
        out = INGEST_DIR / f"facebook_{datetime.now().strftime('%Y%m%d')}.jsonl"
        with open(out, "w") as f:
            for p_item in posts:
                f.write(json.dumps(p_item) + "\n")
        print(f"Wrote {len(posts)} items to {out}")


if __name__ == "__main__":
    main()
