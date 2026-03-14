#!/usr/bin/env python3
"""
Scrape TikTok For You feed from the already-running PWA/tab.
Uses Chrome's AppleScript bridge — zero new browser windows.

Outputs JSONL to ~/.human/feeds/ingest/tiktok_YYYYMMDD.jsonl.

Usage:
    python scripts/scrape_tiktok_feed.py [--max-videos 30]
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
    run_js_in_chrome_tab(url_match, "window.scrollBy(0, window.innerHeight); 'ok'")
    time.sleep(2)


def scrape_tiktok(max_videos: int = 30) -> list[dict]:
    tab_check = run_js_in_chrome_tab("tiktok.com", "document.title")
    if tab_check == "TAB_NOT_FOUND":
        print("Error: No tiktok.com tab found in Chrome. Is TikTok open?")
        return []

    print(f"  Found TikTok tab: {tab_check}")

    js_extract = r"""
    (function() {
        var vids = [];
        var selectors = '[data-e2e=\"browse-video-desc\"], [data-e2e=\"video-desc\"], span.tiktok-j2a19r-SpanText, [class*=\"DivVideoDescription\"] span, [class*=\"SpanText\"]';
        document.querySelectorAll(selectors).forEach(function(el) {
            var text = el.innerText.trim();
            if (text.length > 10) {
                var container = el.closest('[data-e2e=\"recommend-list-item-container\"]');
                var link = container ? container.querySelector('a') : null;
                vids.push(JSON.stringify({
                    source: 'tiktok',
                    content_type: 'video_caption',
                    content: text.substring(0, 2000),
                    url: link ? link.href : ''
                }));
            }
        });
        return vids.join('\\n');
    })()
    """.replace("\n", " ")

    all_videos = {}
    scroll_rounds = max_videos + 10

    for i in range(scroll_rounds):
        if len(all_videos) >= max_videos:
            break

        raw = run_js_in_chrome_tab("tiktok.com", js_extract)
        if raw and raw != "TAB_NOT_FOUND":
            for line in raw.split("\n"):
                line = line.strip()
                if not line:
                    continue
                try:
                    item = json.loads(line)
                    key = item.get("content", "")[:100]
                    if key and key not in all_videos:
                        all_videos[key] = item
                except json.JSONDecodeError:
                    pass

        if len(all_videos) >= max_videos:
            break
        scroll_tab("tiktok.com")

    return list(all_videos.values())[:max_videos]


def main():
    parser = argparse.ArgumentParser(description="Scrape TikTok from running PWA")
    parser.add_argument("--max-videos", type=int, default=30)
    args = parser.parse_args()

    print(f"Scraping up to {args.max_videos} videos from running TikTok tab...")
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
        print("No videos scraped — is TikTok open in Chrome?")


if __name__ == "__main__":
    main()
