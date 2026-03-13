#!/usr/bin/env python3
"""
Open a Playwright browser to a platform, wait for user to log in,
then auto-save session after a timeout. Non-interactive.

Usage:
    python capture_session.py <platform> [--timeout 120]
"""

import sys
import time
from pathlib import Path
from playwright.sync_api import sync_playwright

PLATFORMS = {
    "twitter":  "https://x.com/home",
    "facebook": "https://www.facebook.com/",
    "tiktok":   "https://www.tiktok.com/foryou",
}

STATE_DIR = Path.home() / ".human" / "browser_state"


def capture(platform: str, timeout_sec: int = 120) -> bool:
    url = PLATFORMS[platform]
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    state_file = STATE_DIR / f"{platform}.json"

    print(f"Opening {platform} — log in within {timeout_sec}s, session auto-saves.")

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=False)
        context = browser.new_context(
            storage_state=str(state_file) if state_file.exists() else None,
            viewport={"width": 1280, "height": 900},
        )
        page = context.new_page()

        try:
            page.goto(url, wait_until="domcontentloaded", timeout=30000)
        except Exception as e:
            print(f"Navigation warning: {e}")

        print(f"Browser open. Log in to {platform} now...")
        print(f"Auto-saving in {timeout_sec}s...")

        time.sleep(timeout_sec)

        context.storage_state(path=str(state_file))
        browser.close()

    print(f"Session saved to {state_file}")
    return True


if __name__ == "__main__":
    platform = sys.argv[1] if len(sys.argv) > 1 else None
    timeout = int(sys.argv[2]) if len(sys.argv) > 2 else 90

    if platform not in PLATFORMS:
        print(f"Usage: {sys.argv[0]} <{'|'.join(PLATFORMS)}> [timeout_seconds]")
        sys.exit(1)

    capture(platform, timeout)
