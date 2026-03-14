#!/usr/bin/env python3
"""
Scrape Twitter/X home timeline from the already-running PWA window.
Uses Chrome's AppleScript bridge — zero new browser windows.

Outputs JSONL to ~/.human/feeds/ingest/twitter_YYYYMMDD.jsonl.

Usage:
    python scripts/scrape_twitter_feed.py [--max-tweets 50]
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
    """Scroll down in the matching Chrome tab."""
    run_js_in_chrome_tab(url_match, "window.scrollBy(0, window.innerHeight); 'ok'")
    time.sleep(2)


def scrape_twitter(max_tweets: int = 50) -> list[dict]:
    tab_check = run_js_in_chrome_tab("x.com", "document.title")
    if tab_check == "TAB_NOT_FOUND":
        print("Error: No x.com tab found in Chrome. Is the Twitter PWA running?")
        return []

    print(f"  Found Twitter tab: {tab_check}")

    js_extract = r"""
    (function() {
        var tweets = [];
        document.querySelectorAll('[data-testid=\"tweetText\"]').forEach(function(el) {
            var article = el.closest('article');
            var link = article ? article.querySelector('a[href*=\"/status/\"]') : null;
            tweets.push(JSON.stringify({
                source: 'twitter',
                content_type: 'tweet',
                content: el.innerText.substring(0, 2000),
                url: link ? link.href : ''
            }));
        });
        return tweets.join('\\n');
    })()
    """.replace("\n", " ")

    all_tweets = {}
    scroll_rounds = max_tweets // 5 + 5

    for i in range(scroll_rounds):
        if len(all_tweets) >= max_tweets:
            break

        raw = run_js_in_chrome_tab("x.com", js_extract)
        if raw and raw != "TAB_NOT_FOUND":
            for line in raw.split("\n"):
                line = line.strip()
                if not line:
                    continue
                try:
                    item = json.loads(line)
                    key = item.get("content", "")[:100]
                    if key and key not in all_tweets:
                        all_tweets[key] = item
                except json.JSONDecodeError:
                    pass

        if len(all_tweets) >= max_tweets:
            break
        scroll_tab("x.com")

    return list(all_tweets.values())[:max_tweets]


def main():
    parser = argparse.ArgumentParser(description="Scrape Twitter/X from running PWA")
    parser.add_argument("--max-tweets", type=int, default=50)
    args = parser.parse_args()

    print(f"Scraping up to {args.max_tweets} tweets from running Twitter PWA...")
    tweets = scrape_twitter(args.max_tweets)
    print(f"Scraped {len(tweets)} tweets")

    if tweets:
        INGEST_DIR.mkdir(parents=True, exist_ok=True)
        outfile = INGEST_DIR / f"twitter_{datetime.now().strftime('%Y%m%d')}.jsonl"
        with open(outfile, "w") as f:
            for t in tweets:
                f.write(json.dumps(t) + "\n")
        print(f"Wrote {len(tweets)} items to {outfile}")
    else:
        print("No tweets scraped — is the Twitter PWA open?")


if __name__ == "__main__":
    main()
