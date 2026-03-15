#!/usr/bin/env python3
"""Scrape GitHub trending repos (especially AI/ML) into JSONL for h-uman feed ingestion."""
import json, os, sys, time, datetime, urllib.request, urllib.error, html.parser

OUTPUT_DIR = os.path.expanduser("~/.human/feeds")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "github_trending.jsonl")
GITHUB_TRENDING_URL = "https://github.com/trending?since=daily&spoken_language_code=en"

class TrendingParser(html.parser.HTMLParser):
    def __init__(self):
        super().__init__()
        self.repos = []
        self._in_repo_name = False
        self._in_description = False
        self._in_stars = False
        self._current = {}
        self._tag_stack = []

    def handle_starttag(self, tag, attrs):
        attrs_dict = dict(attrs)
        self._tag_stack.append(tag)
        if tag == "article" and "Box-row" in attrs_dict.get("class", ""):
            self._current = {"name": "", "description": "", "language": "", "stars_today": ""}
        if tag == "h2" and "h3" in attrs_dict.get("class", ""):
            self._in_repo_name = True
        if tag == "a" and self._in_repo_name:
            href = attrs_dict.get("href", "")
            if href.startswith("/") and href.count("/") == 2:
                self._current["name"] = href.lstrip("/")
                self._current["url"] = f"https://github.com{href}"
        if tag == "p" and "col-9" in attrs_dict.get("class", ""):
            self._in_description = True

    def handle_endtag(self, tag):
        if self._tag_stack:
            self._tag_stack.pop()
        if tag == "h2":
            self._in_repo_name = False
        if tag == "p" and self._in_description:
            self._in_description = False
        if tag == "article" and self._current.get("name"):
            self.repos.append(self._current)
            self._current = {}

    def handle_data(self, data):
        text = data.strip()
        if not text:
            return
        if self._in_description and self._current:
            self._current["description"] = (self._current.get("description", "") + " " + text).strip()
        if "stars today" in text.lower() and self._current:
            self._current["stars_today"] = text.strip()

AI_KEYWORDS = {
    "ai", "llm", "gpt", "claude", "gemini", "llama", "agent", "transformer",
    "neural", "machine-learning", "deep-learning", "embedding", "rag",
    "inference", "quantization", "diffusion", "chatbot", "copilot",
    "fine-tuning", "langchain", "openai", "anthropic", "huggingface",
}

def is_ai_related(name, desc):
    text = f"{name} {desc}".lower()
    return any(kw in text for kw in AI_KEYWORDS)

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    scrape_ts = datetime.datetime.now(datetime.timezone.utc).isoformat()

    req = urllib.request.Request(GITHUB_TRENDING_URL, headers={
        "User-Agent": "h-uman-feed/1.0",
        "Accept": "text/html",
    })

    try:
        with urllib.request.urlopen(req, timeout=15) as r:
            page_html = r.read().decode("utf-8", errors="replace")
    except Exception as e:
        print(f"[github_trending] ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    parser = TrendingParser()
    parser.feed(page_html)

    items = []
    for repo in parser.repos:
        name = repo.get("name", "")
        desc = repo.get("description", "")[:500]
        items.append({
            "source": "github_trending",
            "content_type": "repo",
            "content": f"{name}: {desc}"[:2000],
            "url": repo.get("url", f"https://github.com/{name}"),
            "author": name.split("/")[0] if "/" in name else "",
            "stars_today": repo.get("stars_today", ""),
            "ai_relevant": is_ai_related(name, desc),
            "scraped_at": scrape_ts,
        })

    ai_items = [i for i in items if i["ai_relevant"]]
    all_items = ai_items + [i for i in items if not i["ai_relevant"]][:10]

    with open(OUTPUT_FILE, "w") as f:
        for item in all_items:
            f.write(json.dumps(item) + "\n")

    print(f"[github_trending] {len(all_items)} repos ({len(ai_items)} AI-relevant) -> {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
