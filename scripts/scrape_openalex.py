#!/usr/bin/env python3
"""Scrape OpenAlex API for recent AI/ML academic works.

OpenAlex is fully open (no key needed). Covers 250M+ works across all fields.
We filter by AI/ML concepts and recent publication date to find relevant papers.
"""
import json, os, sys, datetime, urllib.request, urllib.parse, time

OUTPUT_DIR = os.path.expanduser("~/.human/feeds/ingest")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "openalex.jsonl")

OPENALEX_API = "https://api.openalex.org/works"

CONCEPT_IDS = [
    "C154945302",  # Artificial intelligence
    "C108827166",  # Natural language processing
    "C119857082",  # Machine learning
    "C50644808",   # Artificial neural network
]

SEARCH_QUERIES = [
    "large language model agent",
    "retrieval augmented generation",
    "AI code generation",
    "LLM inference optimization",
    "multi-agent system",
]


def fetch_openalex_concepts(concept_id, per_page=10):
    today = datetime.date.today()
    from_date = (today - datetime.timedelta(days=14)).isoformat()
    params = urllib.parse.urlencode({
        "filter": f"concepts.id:{concept_id},from_publication_date:{from_date}",
        "sort": "cited_by_count:desc",
        "per_page": per_page,
    })
    url = f"{OPENALEX_API}?{params}"
    try:
        req = urllib.request.Request(url, headers={
            "User-Agent": "h-uman-feed/1.0 (mailto:seth@h-uman.ai)",
        })
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.loads(r.read())
    except Exception as e:
        print(f"[openalex] Error fetching concept {concept_id}: {e}", file=sys.stderr)
        return None


def fetch_openalex_search(query, per_page=5):
    today = datetime.date.today()
    from_date = (today - datetime.timedelta(days=30)).isoformat()
    params = urllib.parse.urlencode({
        "search": query,
        "filter": f"from_publication_date:{from_date}",
        "sort": "cited_by_count:desc",
        "per_page": per_page,
    })
    url = f"{OPENALEX_API}?{params}"
    try:
        req = urllib.request.Request(url, headers={
            "User-Agent": "h-uman-feed/1.0 (mailto:seth@h-uman.ai)",
        })
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.loads(r.read())
    except Exception as e:
        print(f"[openalex] Error searching '{query}': {e}", file=sys.stderr)
        return None


def extract_works(data, seen_ids):
    items = []
    if not data or "results" not in data:
        return items

    for work in data["results"]:
        wid = work.get("id", "")
        if not wid or wid in seen_ids:
            continue
        seen_ids.add(wid)

        title = work.get("title", "")
        if not title:
            continue

        abstract_inv = work.get("abstract_inverted_index")
        abstract = ""
        if abstract_inv and isinstance(abstract_inv, dict):
            word_positions = []
            for word, positions in abstract_inv.items():
                for pos in positions:
                    word_positions.append((pos, word))
            word_positions.sort()
            abstract = " ".join(w for _, w in word_positions)

        authorships = work.get("authorships", []) or []
        authors = []
        for a in authorships[:5]:
            author_obj = a.get("author", {})
            name = author_obj.get("display_name", "")
            if name:
                authors.append(name)
        author_str = ", ".join(authors[:3])
        if len(authors) > 3:
            author_str += f" +{len(authors) - 3} more"

        citations = work.get("cited_by_count", 0) or 0
        pub_date = work.get("publication_date", "") or ""
        doi = work.get("doi", "") or ""
        url = doi if doi else wid

        concepts = []
        for c in (work.get("concepts") or [])[:5]:
            cname = c.get("display_name", "")
            if cname:
                concepts.append(cname)

        content = f"{title}\n\nAuthors: {author_str}"
        if citations:
            content += f"\nCitations: {citations}"
        if concepts:
            content += f"\nTopics: {', '.join(concepts)}"
        if abstract:
            content += f"\n\n{abstract}"

        items.append({
            "source": "openalex",
            "content_type": "paper",
            "content": content[:2000],
            "url": url,
            "author": author_str,
            "title": title,
            "citations": citations,
            "published": pub_date,
            "concepts": concepts,
        })

    return items


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    scrape_ts = datetime.datetime.now(datetime.timezone.utc).isoformat()
    all_items = []
    seen_ids = set()

    for cid in CONCEPT_IDS:
        data = fetch_openalex_concepts(cid, per_page=5)
        items = extract_works(data, seen_ids)
        for item in items:
            item["scraped_at"] = scrape_ts
            all_items.append(item)
        time.sleep(0.5)

    for query in SEARCH_QUERIES:
        data = fetch_openalex_search(query, per_page=5)
        items = extract_works(data, seen_ids)
        for item in items:
            item["scraped_at"] = scrape_ts
            all_items.append(item)
        time.sleep(0.5)

    all_items.sort(key=lambda x: x.get("citations", 0), reverse=True)

    with open(OUTPUT_FILE, "w") as f:
        for item in all_items:
            f.write(json.dumps(item) + "\n")

    print(f"[openalex] {len(all_items)} works -> {OUTPUT_FILE}")


if __name__ == "__main__":
    main()
