#!/usr/bin/env python3
"""Scrape Semantic Scholar API for trending AI papers.

Uses the Semantic Scholar Academic Graph API — free, no key needed for basic use
(100 req/sec). Searches for recent highly-cited AI papers with abstracts,
citation counts, and influence scores.
"""
import json, os, sys, datetime, urllib.request, urllib.parse, time

OUTPUT_DIR = os.path.expanduser("~/.human/feeds/ingest")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "semantic_scholar.jsonl")

S2_API = "https://api.semanticscholar.org/graph/v1/paper/search"
S2_FIELDS = "title,abstract,authors,year,citationCount,influentialCitationCount,url,publicationDate,externalIds"

QUERIES = [
    "autonomous AI agent tool use",
    "large language model inference optimization",
    "retrieval augmented generation",
    "multi-agent LLM system",
    "AI code generation assistant",
    "model context protocol",
    "LLM safety alignment RLHF",
    "vector embedding semantic search",
    "AI agent memory architecture",
    "edge inference optimization quantization",
]


def fetch_s2(query, limit=10):
    params = urllib.parse.urlencode({
        "query": query,
        "limit": limit,
        "fields": S2_FIELDS,
        "sort": "citationCount:desc",
        "year": "2024-2026",
    })
    url = f"{S2_API}?{params}"
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "h-uman-feed/1.0"})
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.loads(r.read())
    except Exception as e:
        print(f"[semantic_scholar] Error fetching '{query}': {e}", file=sys.stderr)
        return None


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    scrape_ts = datetime.datetime.now(datetime.timezone.utc).isoformat()
    all_items = []
    seen_ids = set()

    for i, query in enumerate(QUERIES):
        if i > 0:
            time.sleep(3)

        data = fetch_s2(query, limit=5)
        if not data or "data" not in data:
            continue

        for paper in data["data"]:
            pid = paper.get("paperId", "")
            if not pid or pid in seen_ids:
                continue
            seen_ids.add(pid)

            title = paper.get("title", "")
            abstract = paper.get("abstract", "") or ""
            if not title:
                continue

            authors = []
            for a in (paper.get("authors") or [])[:5]:
                name = a.get("name", "")
                if name:
                    authors.append(name)
            author_str = ", ".join(authors[:3])
            if len(authors) > 3:
                author_str += f" +{len(authors) - 3} more"

            citations = paper.get("citationCount", 0) or 0
            influential = paper.get("influentialCitationCount", 0) or 0
            pub_date = paper.get("publicationDate", "") or ""
            year = paper.get("year", "") or ""
            url = paper.get("url", "") or f"https://www.semanticscholar.org/paper/{pid}"

            ext_ids = paper.get("externalIds") or {}
            arxiv_id = ext_ids.get("ArXiv", "")
            doi = ext_ids.get("DOI", "")

            content = f"{title}\n\nAuthors: {author_str}"
            if citations:
                content += f"\nCitations: {citations}"
            if influential:
                content += f" ({influential} influential)"
            if abstract:
                content += f"\n\n{abstract}"

            all_items.append({
                "source": "semantic_scholar",
                "content_type": "paper",
                "content": content[:2000],
                "url": url,
                "author": author_str,
                "title": title,
                "citations": citations,
                "influential_citations": influential,
                "published": pub_date or str(year),
                "arxiv_id": arxiv_id,
                "doi": doi,
                "scraped_at": scrape_ts,
            })

    all_items.sort(key=lambda x: x.get("citations", 0), reverse=True)

    with open(OUTPUT_FILE, "w") as f:
        for item in all_items:
            f.write(json.dumps(item) + "\n")

    print(f"[semantic_scholar] {len(all_items)} papers from {len(QUERIES)} queries -> {OUTPUT_FILE}")


if __name__ == "__main__":
    main()
