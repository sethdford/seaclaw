#!/usr/bin/env python3
"""
Scrape Facebook news feed from the already-running PWA/tab.
Uses Chrome's AppleScript bridge — zero new browser windows.

Outputs JSONL to ~/.human/feeds/ingest/facebook_YYYYMMDD.jsonl.

Usage:
    python scripts/scrape_facebook_feed.py [--max-posts 30]
"""

import argparse
import json
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

INGEST_DIR = Path.home() / ".human" / "feeds" / "ingest"


def run_js_in_chrome_tab(url_match: str, js: str) -> str:
    """Execute JavaScript in the Chrome tab whose URL contains url_match."""
    script = f'''
    tell application "Google Chrome"
        repeat with w in every window
            repeat with t in every tab of w
                if URL of t contains "{url_match}" then
                    return execute t javascript "{js}"
                end if
            end repeat
        end repeat
        return "TAB_NOT_FOUND"
    end tell
    '''
    result = subprocess.run(
        ["osascript", "-e", script],
        capture_output=True, text=True, timeout=30,
    )
    if result.returncode != 0:
        raise RuntimeError(f"AppleScript error: {result.stderr.strip()}")
    return result.stdout.strip()


def scroll_tab(url_match: str) -> None:
    run_js_in_chrome_tab(url_match, "window.scrollBy(0, window.innerHeight * 1.5); 'ok'")
    time.sleep(2)


def scrape_facebook(max_posts: int = 30) -> list[dict]:
    tab_check = run_js_in_chrome_tab("facebook.com", "document.title")
    if tab_check == "TAB_NOT_FOUND":
        print("Error: No facebook.com tab found in Chrome. Is Facebook open?")
        return []

    print(f"  Found Facebook tab: {tab_check}")

    js_extract = r"""
    (function() {
        var posts = [];
        var selectors = '[data-ad-comet-preview=\"message\"], [data-ad-preview=\"message\"], div[dir=\"auto\"][style*=\"text-align\"]';
        document.querySelectorAll(selectors).forEach(function(el) {
            var text = el.innerText.trim();
            if (text.length > 20) {
                posts.push(JSON.stringify({
                    source: 'facebook',
                    content_type: 'post',
                    content: text.substring(0, 2000),
                    url: ''
                }));
            }
        });
        return posts.join('\\n');
    })()
    """.replace("\n", " ")

    all_posts = {}
    scroll_rounds = max_posts // 3 + 10

    for i in range(scroll_rounds):
        if len(all_posts) >= max_posts:
            break

        raw = run_js_in_chrome_tab("facebook.com", js_extract)
        if raw and raw != "TAB_NOT_FOUND":
            for line in raw.split("\n"):
                line = line.strip()
                if not line:
                    continue
                try:
                    item = json.loads(line)
                    key = item.get("content", "")[:100]
                    if key and key not in all_posts:
                        all_posts[key] = item
                except json.JSONDecodeError:
                    pass

        if len(all_posts) >= max_posts:
            break
        scroll_tab("facebook.com")

    return list(all_posts.values())[:max_posts]


def main():
    parser = argparse.ArgumentParser(description="Scrape Facebook from running PWA")
    parser.add_argument("--max-posts", type=int, default=30)
    args = parser.parse_args()

    print(f"Scraping up to {args.max_posts} posts from running Facebook tab...")
    posts = scrape_facebook(args.max_posts)
    print(f"Scraped {len(posts)} posts")

    if posts:
        INGEST_DIR.mkdir(parents=True, exist_ok=True)
        outfile = INGEST_DIR / f"facebook_{datetime.now().strftime('%Y%m%d')}.jsonl"
        with open(outfile, "w") as f:
            for p_item in posts:
                f.write(json.dumps(p_item) + "\n")
        print(f"Wrote {len(posts)} items to {outfile}")
    else:
        print("No posts scraped — is Facebook open in Chrome?")


if __name__ == "__main__":
    main()
