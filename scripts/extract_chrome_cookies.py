#!/usr/bin/env python3
"""
Extract cookies from the local Chrome installation (including PWA apps)
and convert them to Playwright storage_state JSON files.

This means you only need to log in once in Chrome/PWA — no separate
Playwright login required.

Usage:
    python scripts/extract_chrome_cookies.py              # all platforms
    python scripts/extract_chrome_cookies.py twitter      # just twitter
"""

import json
import sys
from pathlib import Path

from pycookiecheat import chrome_cookies

PLATFORMS = {
    "twitter": [
        "https://x.com",
        "https://twitter.com",
    ],
    "facebook": [
        "https://www.facebook.com",
    ],
    "tiktok": [
        "https://www.tiktok.com",
    ],
}

STATE_DIR = Path.home() / ".human" / "browser_state"


def cookies_to_playwright_state(all_cookies: list[dict]) -> dict:
    """Convert cookie dicts to Playwright storage_state format."""
    pw_cookies = []
    for c in all_cookies:
        pw_cookies.append({
            "name": c["name"],
            "value": c["value"],
            "domain": c["domain"],
            "path": c.get("path", "/"),
            "expires": c.get("expires", -1),
            "httpOnly": c.get("httpOnly", False),
            "secure": c.get("secure", True),
            "sameSite": c.get("sameSite", "Lax"),
        })
    return {"cookies": pw_cookies, "origins": []}


def extract_for_platform(platform: str) -> int:
    """Extract Chrome cookies for a platform. Returns cookie count."""
    urls = PLATFORMS[platform]
    all_cookies = []

    for url in urls:
        try:
            jar = chrome_cookies(url, cookie_file=None)
            for name, value in jar.items():
                domain = url.replace("https://", "").replace("http://", "")
                all_cookies.append({
                    "name": name,
                    "value": value,
                    "domain": f".{domain}" if not domain.startswith(".") else domain,
                    "path": "/",
                    "expires": -1,
                    "httpOnly": False,
                    "secure": True,
                    "sameSite": "Lax",
                })
        except Exception as e:
            print(f"  Warning: couldn't read cookies for {url}: {e}")

    if not all_cookies:
        print(f"  No cookies found for {platform} — are you logged in via Chrome/PWA?")
        return 0

    STATE_DIR.mkdir(parents=True, exist_ok=True)
    state_file = STATE_DIR / f"{platform}.json"
    state = cookies_to_playwright_state(all_cookies)

    with open(state_file, "w") as f:
        json.dump(state, f, indent=2)

    print(f"  Extracted {len(all_cookies)} cookies -> {state_file}")
    return len(all_cookies)


def main():
    targets = sys.argv[1:] if len(sys.argv) > 1 else list(PLATFORMS.keys())

    for t in targets:
        if t not in PLATFORMS:
            print(f"Unknown platform: {t} (available: {', '.join(PLATFORMS)})")
            continue
        print(f"Extracting {t} cookies from Chrome...")
        extract_for_platform(t)

    print("\nDone. Scrapers will use these cookies automatically.")


if __name__ == "__main__":
    main()
