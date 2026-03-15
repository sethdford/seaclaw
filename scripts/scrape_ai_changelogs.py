#!/usr/bin/env python3
"""Monitor AI provider changelogs and docs for new features.

Scrapes Anthropic, OpenAI, and Google AI changelogs/release notes to detect
new API capabilities h-uman could integrate. Outputs JSONL.
"""
import json, os, sys, datetime, urllib.request, urllib.error, html.parser, re, hashlib

FEEDS_DIR = os.path.expanduser("~/.human/feeds")
OUTPUT_DIR = os.path.expanduser("~/.human/feeds/ingest")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "ai_changelogs.jsonl")
SEEN_FILE = os.path.join(FEEDS_DIR, ".changelog_seen.json")

SOURCES = [
    {
        "name": "anthropic",
        "url": "https://docs.anthropic.com/en/docs/about-claude/models",
        "changelog_url": "https://docs.anthropic.com/en/api/changelog",
        "display": "Anthropic API Changelog",
    },
    {
        "name": "openai",
        "url": "https://platform.openai.com/docs/changelog",
        "display": "OpenAI API Changelog",
    },
    {
        "name": "google_ai",
        "url": "https://ai.google.dev/gemini-api/docs/changelog",
        "display": "Google Gemini API Changelog",
    },
    {
        "name": "mistral",
        "url": "https://docs.mistral.ai/getting-started/changelog/",
        "display": "Mistral AI Changelog",
    },
    {
        "name": "huggingface",
        "url": "https://huggingface.co/blog",
        "display": "Hugging Face Blog",
    },
]

class ChangelogParser(html.parser.HTMLParser):
    """Extract text content from changelog HTML pages."""
    def __init__(self):
        super().__init__()
        self.sections = []
        self._current_section = ""
        self._in_heading = False
        self._heading_level = 0
        self._skip_tags = {"script", "style", "nav", "footer", "header"}
        self._skip_depth = 0

    def handle_starttag(self, tag, attrs):
        if tag in self._skip_tags:
            self._skip_depth += 1
        if tag in ("h1", "h2", "h3", "h4") and self._skip_depth == 0:
            self._in_heading = True
            if self._current_section.strip():
                self.sections.append(self._current_section.strip())
            self._current_section = ""

    def handle_endtag(self, tag):
        if tag in self._skip_tags and self._skip_depth > 0:
            self._skip_depth -= 1
        if tag in ("h1", "h2", "h3", "h4"):
            self._in_heading = False
            self._current_section += "\n"

    def handle_data(self, data):
        if self._skip_depth > 0:
            return
        text = data.strip()
        if text:
            self._current_section += " " + text

    def close(self):
        super().close()
        if self._current_section.strip():
            self.sections.append(self._current_section.strip())

def load_seen():
    if os.path.exists(SEEN_FILE):
        with open(SEEN_FILE) as f:
            return json.load(f)
    return {}

def save_seen(seen):
    with open(SEEN_FILE, 'w') as f:
        json.dump(seen, f)

def fetch_page(url):
    req = urllib.request.Request(url, headers={
        "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) h-uman-feed/1.0",
        "Accept": "text/html",
    })
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return r.read().decode("utf-8", errors="replace")
    except Exception as e:
        print(f"  [warn] Could not fetch {url}: {e}", file=sys.stderr)
        return None

def extract_changes(html_content, source_name):
    parser = ChangelogParser()
    parser.feed(html_content)
    parser.close()

    date_pattern = re.compile(r'(20\d{2}[-/]\d{1,2}[-/]\d{1,2}|(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)\w*\s+\d{1,2},?\s+20\d{2})', re.IGNORECASE)
    changes = []

    for section in parser.sections[:30]:
        if len(section) < 20:
            continue
        date_match = date_pattern.search(section)
        date_str = date_match.group(0) if date_match else ""
        content_hash = hashlib.md5(section[:200].encode()).hexdigest()[:12]
        changes.append({
            "content": section[:1000],
            "date": date_str,
            "hash": content_hash,
        })

    return changes

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    scrape_ts = datetime.datetime.now(datetime.timezone.utc).isoformat()
    seen = load_seen()
    items = []

    for source in SOURCES:
        name = source["name"]
        urls_to_try = [source.get("changelog_url", source["url"]), source["url"]]

        for url in urls_to_try:
            if not url:
                continue
            html_content = fetch_page(url)
            if not html_content:
                continue

            changes = extract_changes(html_content, name)
            source_seen = set(seen.get(name, []))

            new_changes = [c for c in changes if c["hash"] not in source_seen]

            for change in new_changes[:10]:
                items.append({
                    "source": f"changelog_{name}",
                    "content_type": "changelog",
                    "content": f"[{source['display']}] {change['content']}"[:2000],
                    "url": url,
                    "author": source["display"],
                    "date": change["date"],
                    "scraped_at": scrape_ts,
                })

            seen[name] = list(source_seen | {c["hash"] for c in changes})[-100:]
            if new_changes:
                break

    save_seen(seen)

    with open(OUTPUT_FILE, "w") as f:
        for item in items:
            f.write(json.dumps(item) + "\n")

    print(f"[changelogs] {len(items)} new changelog entries -> {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
