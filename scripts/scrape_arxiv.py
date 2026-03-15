#!/usr/bin/env python3
"""Scrape arXiv API for recent AI/ML papers with full abstracts.

Uses the arXiv API (export.arxiv.org/api/query) — free, no key needed.
Searches cs.AI, cs.CL, cs.LG, cs.MA categories for recent papers matching
AI agent, LLM, and systems-level keywords relevant to h-uman.
"""
import json, os, sys, datetime, urllib.request, urllib.parse
import xml.etree.ElementTree as ET

OUTPUT_DIR = os.path.expanduser("~/.human/feeds/ingest")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "arxiv.jsonl")

ARXIV_API = "http://export.arxiv.org/api/query"

QUERIES = [
    "all:autonomous AI agent",
    "all:large language model tool use",
    "all:retrieval augmented generation",
    "all:multi-agent system LLM",
    "all:AI code generation",
    "all:model context protocol",
    "all:LLM inference optimization",
    "all:AI safety alignment",
    "all:embedding vector search",
    "all:edge AI embedded inference",
]

CATEGORIES = "cat:cs.AI OR cat:cs.CL OR cat:cs.LG OR cat:cs.MA"
NS = {"atom": "http://www.w3.org/2005/Atom", "arxiv": "http://arxiv.org/schemas/atom"}


def fetch_arxiv(query, max_results=10):
    params = urllib.parse.urlencode({
        "search_query": f"({query}) AND ({CATEGORIES})",
        "start": 0,
        "max_results": max_results,
        "sortBy": "submittedDate",
        "sortOrder": "descending",
    })
    url = f"{ARXIV_API}?{params}"
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "h-uman-feed/1.0"})
        with urllib.request.urlopen(req, timeout=30) as r:
            return ET.fromstring(r.read())
    except Exception as e:
        print(f"[arxiv] Error fetching '{query}': {e}", file=sys.stderr)
        return None


def parse_entries(root):
    entries = []
    for entry in root.findall("atom:entry", NS):
        paper_id = entry.findtext("atom:id", "", NS)
        title = entry.findtext("atom:title", "", NS).strip().replace("\n", " ")
        abstract = entry.findtext("atom:summary", "", NS).strip().replace("\n", " ")
        published = entry.findtext("atom:published", "", NS)

        authors = []
        for author in entry.findall("atom:author", NS):
            name = author.findtext("atom:name", "", NS)
            if name:
                authors.append(name)

        categories = []
        for cat in entry.findall("atom:category", NS):
            term = cat.get("term", "")
            if term:
                categories.append(term)

        pdf_url = ""
        for link in entry.findall("atom:link", NS):
            if link.get("title") == "pdf":
                pdf_url = link.get("href", "")
                break

        if title and abstract:
            entries.append({
                "id": paper_id,
                "title": title,
                "abstract": abstract[:1500],
                "authors": authors[:5],
                "categories": categories,
                "published": published,
                "pdf_url": pdf_url,
                "url": paper_id,
            })
    return entries


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    scrape_ts = datetime.datetime.now(datetime.timezone.utc).isoformat()
    all_items = []
    seen_ids = set()

    for query in QUERIES:
        root = fetch_arxiv(query, max_results=5)
        if root is None:
            continue
        entries = parse_entries(root)
        for e in entries:
            if e["id"] in seen_ids:
                continue
            seen_ids.add(e["id"])
            author_str = ", ".join(e["authors"][:3])
            if len(e["authors"]) > 3:
                author_str += f" +{len(e['authors']) - 3} more"
            content = f"{e['title']}\n\nAuthors: {author_str}\nCategories: {', '.join(e['categories'])}\n\n{e['abstract']}"
            all_items.append({
                "source": "arxiv",
                "content_type": "paper",
                "content": content[:2000],
                "url": e["url"],
                "author": author_str,
                "title": e["title"],
                "published": e["published"],
                "categories": e["categories"],
                "scraped_at": scrape_ts,
            })

    with open(OUTPUT_FILE, "w") as f:
        for item in all_items:
            f.write(json.dumps(item) + "\n")

    print(f"[arxiv] {len(all_items)} papers from {len(QUERIES)} queries -> {OUTPUT_FILE}")


if __name__ == "__main__":
    main()
