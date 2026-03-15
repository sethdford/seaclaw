#!/usr/bin/env python3
"""Scrape Hacker News front page and top AI/LLM stories into JSONL for h-uman feed ingestion."""
import json, os, sys, time, datetime, urllib.request, urllib.error

OUTPUT_DIR = os.path.expanduser("~/.human/feeds/ingest")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "hackernews.jsonl")
HN_TOP = "https://hacker-news.firebaseio.com/v0/topstories.json"
HN_ITEM = "https://hacker-news.firebaseio.com/v0/item/{}.json"
MAX_STORIES = 60
AI_KEYWORDS = {
    "ai", "llm", "gpt", "claude", "gemini", "llama", "mistral", "anthropic",
    "openai", "deepmind", "agent", "autonomous", "embedding", "vector",
    "transformer", "neural", "machine learning", "deep learning", "fine-tune",
    "rlhf", "rag", "inference", "quantization", "mcp", "tool-use", "function calling",
    "prompt", "chain-of-thought", "reasoning", "code generation", "copilot",
}

def fetch_json(url, retries=3):
    for i in range(retries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "h-uman-feed/1.0"})
            with urllib.request.urlopen(req, timeout=10) as r:
                return json.loads(r.read())
        except (urllib.error.URLError, OSError) as e:
            if i == retries - 1:
                raise
            time.sleep(2 ** i)

def is_ai_related(title):
    lower = title.lower()
    return any(kw in lower for kw in AI_KEYWORDS)

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    scrape_ts = datetime.datetime.now(datetime.timezone.utc).isoformat()
    top_ids = fetch_json(HN_TOP)[:MAX_STORIES]
    items = []

    for sid in top_ids:
        try:
            story = fetch_json(HN_ITEM.format(sid))
        except Exception:
            continue
        if not story or story.get("type") != "story":
            continue
        title = story.get("title", "")
        if not title:
            continue

        items.append({
            "source": "hackernews",
            "content_type": "link",
            "content": title[:2000],
            "url": story.get("url", f"https://news.ycombinator.com/item?id={sid}"),
            "author": story.get("by", ""),
            "score": story.get("score", 0),
            "comments": story.get("descendants", 0),
            "ai_relevant": is_ai_related(title),
            "scraped_at": scrape_ts,
        })

    ai_items = [i for i in items if i["ai_relevant"]]
    all_items = ai_items + [i for i in items if not i["ai_relevant"]][:20]

    with open(OUTPUT_FILE, "w") as f:
        for item in all_items:
            f.write(json.dumps(item) + "\n")

    print(f"[hackernews] {len(all_items)} stories ({len(ai_items)} AI-relevant) -> {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
