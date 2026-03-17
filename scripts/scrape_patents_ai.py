#!/usr/bin/env python3
"""Monitor USPTO and Google Patents for AI-related patent filings.

Uses Google Patents public RSS and USPTO PAIR API to find recent AI/ML patent
applications that could signal upcoming technology trends.

Note: Google Patents RSS (patents.google.com/rss/search) returns 404 as of 2026.
The script will produce 0 items until an alternative source is integrated.
"""
import json, os, sys, datetime, urllib.request, urllib.parse, xml.etree.ElementTree as ET

OUTPUT_DIR = os.path.expanduser("~/.human/feeds/ingest")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "patents_ai.jsonl")

PATENT_QUERIES = [
    "large language model",
    "autonomous AI agent",
    "neural network inference optimization",
    "transformer architecture",
    "retrieval augmented generation",
    "AI function calling tool use",
    "edge AI embedded inference",
    "multi-agent system",
]

GOOGLE_PATENTS_RSS = "https://patents.google.com/rss/search?q={}&num=10&oq={}"

def fetch_patent_rss(query):
    q = urllib.parse.quote(query)
    url = GOOGLE_PATENTS_RSS.format(q, q)
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "h-uman-feed/1.0"})
        with urllib.request.urlopen(req, timeout=15) as r:
            content = r.read()
            try:
                return ET.fromstring(content)
            except ET.ParseError:
                return None
    except Exception:
        return None

def fetch_patent_search_html(query):
    """Fallback: scrape Google Patents search results page."""
    q = urllib.parse.quote(query)
    url = f"https://patents.google.com/?q=({q})&oq={q}"
    try:
        req = urllib.request.Request(url, headers={
            "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) h-uman-feed/1.0",
            "Accept": "text/html",
        })
        with urllib.request.urlopen(req, timeout=15) as r:
            return r.read().decode("utf-8", errors="replace")
    except Exception:
        return None

def parse_rss_items(root, query):
    items = []
    channel = root.find("channel")
    if channel is None:
        return items

    for item in channel.findall("item")[:5]:
        title = item.findtext("title", "")
        link = item.findtext("link", "")
        description = item.findtext("description", "")
        pub_date = item.findtext("pubDate", "")

        if title:
            items.append({
                "title": title[:500],
                "url": link,
                "description": description[:500],
                "pub_date": pub_date,
                "query": query,
            })

    return items

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    scrape_ts = datetime.datetime.now(datetime.timezone.utc).isoformat()
    all_items = []
    seen_urls = set()

    for query in PATENT_QUERIES:
        root = fetch_patent_rss(query)
        if root is not None:
            patents = parse_rss_items(root, query)
            for p in patents:
                if p["url"] not in seen_urls:
                    seen_urls.add(p["url"])
                    all_items.append({
                        "source": "patents",
                        "content_type": "patent",
                        "content": f"{p['title']}\n\n{p['description']}"[:2000],
                        "url": p["url"],
                        "author": "",
                        "query": p["query"],
                        "pub_date": p["pub_date"],
                        "scraped_at": scrape_ts,
                    })

    if not all_items:
        print("[patents] RSS returned 0 results, trying HTML fallback...", file=sys.stderr)
        import re
        for query in PATENT_QUERIES[:3]:
            html = fetch_patent_search_html(query)
            if not html:
                continue
            for m in re.finditer(r'<article[^>]*>.*?<a[^>]*href="(/patent/[^"]+)"[^>]*>([^<]+)</a>', html, re.DOTALL):
                link = "https://patents.google.com" + m.group(1)
                title = m.group(2).strip()
                if link not in seen_urls and title:
                    seen_urls.add(link)
                    all_items.append({
                        "source": "patents",
                        "content_type": "patent",
                        "content": title[:2000],
                        "url": link,
                        "author": "",
                        "query": query,
                        "pub_date": "",
                        "scraped_at": scrape_ts,
                    })

    with open(OUTPUT_FILE, "w") as f:
        for item in all_items:
            f.write(json.dumps(item) + "\n")

    print(f"[patents] {len(all_items)} AI patents from {len(PATENT_QUERIES)} queries -> {OUTPUT_FILE}")
    if not all_items:
        sys.exit(1)

if __name__ == "__main__":
    main()
