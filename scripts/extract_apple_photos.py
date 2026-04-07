#!/usr/bin/env python3
"""
Extract Apple Photos metadata for persona fine-tuning.

Reads the Photos.sqlite database to extract:
1. Photo descriptions/captions (ZTITLE, ZDESCRIPTION from ZASSET)
2. Memory stories and titles (ZTITLE from ZMEMORY)
3. Place/location context (reverse geocoded place names)
4. People associations (named faces via ZPERSON)
5. Shared album comments and captions

This data enriches the persona with lived experiences — places visited,
people photographed, moments captured — without including any actual images.

Output: data/photos/photo_context.jsonl — grounding data for persona training

Usage:
    python3 scripts/extract_apple_photos.py [--limit 5000]
    python3 scripts/extract_apple_photos.py --memories-only
    python3 scripts/extract_apple_photos.py --stats
"""

import json
import os
import sqlite3
import sys
from collections import Counter, defaultdict
from datetime import datetime

PHOTOS_DB = os.path.expanduser(
    "~/Pictures/Photos Library.photoslibrary/database/Photos.sqlite"
)
PHOTOS_DB_ALT = os.path.expanduser(
    "~/Pictures/Photos Library.photoslibrary/database/photos.db"
)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(SCRIPT_DIR, "..", "data", "photos")

APPLE_EPOCH = 978307200  # 2001-01-01 00:00:00 UTC


def apple_date_to_iso(apple_ts):
    if not apple_ts or apple_ts == 0:
        return ""
    try:
        unix_ts = APPLE_EPOCH + apple_ts
        return datetime.fromtimestamp(unix_ts).isoformat()
    except (OSError, ValueError):
        return ""


def apple_date_to_year(apple_ts):
    if not apple_ts or apple_ts == 0:
        return 0
    try:
        unix_ts = APPLE_EPOCH + apple_ts
        return datetime.fromtimestamp(unix_ts).year
    except (OSError, ValueError):
        return 0


def connect_db():
    for path in [PHOTOS_DB, PHOTOS_DB_ALT]:
        if os.path.exists(path):
            try:
                conn = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
                conn.row_factory = sqlite3.Row
                conn.execute("SELECT 1 FROM ZASSET LIMIT 1")
                print(f"  Connected to {path}")
                return conn
            except Exception as e:
                print(f"  Could not read {path}: {e}")
    return None


def table_exists(conn, table_name):
    row = conn.execute(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?",
        (table_name,),
    ).fetchone()
    return row[0] > 0


def extract_memories(conn):
    """Extract photo Memories — curated story titles and subtitles."""
    if not table_exists(conn, "ZMEMORY"):
        return []
    memories = []
    try:
        rows = conn.execute("""
            SELECT ZTITLE, ZSUBTITLE, ZSTARTDATE, ZENDDATE,
                   ZCATEGORY, ZSUBCATEGORY, ZSCORE
            FROM ZMEMORY
            WHERE ZTITLE IS NOT NULL AND ZTITLE != ''
            ORDER BY ZSTARTDATE DESC
        """).fetchall()
        for row in rows:
            start = apple_date_to_iso(row["ZSTARTDATE"])
            end = apple_date_to_iso(row["ZENDDATE"])
            memories.append({
                "type": "memory",
                "title": row["ZTITLE"],
                "subtitle": row["ZSUBTITLE"] or "",
                "start_date": start,
                "end_date": end,
                "category": row["ZCATEGORY"] or 0,
                "score": row["ZSCORE"] or 0,
            })
    except Exception as e:
        print(f"  Warning: could not read ZMEMORY: {e}")
    return memories


def extract_people(conn):
    """Extract recognized people (named faces)."""
    if not table_exists(conn, "ZPERSON"):
        return []
    people = []
    try:
        rows = conn.execute("""
            SELECT ZDISPLAYNAME, ZFULLNAME, ZFACECOUNT, ZTYPE,
                   ZCONTACTMATCHINGDICTIONARY
            FROM ZPERSON
            WHERE (ZDISPLAYNAME IS NOT NULL AND ZDISPLAYNAME != '')
               OR (ZFULLNAME IS NOT NULL AND ZFULLNAME != '')
            ORDER BY ZFACECOUNT DESC
        """).fetchall()
        for row in rows:
            name = row["ZDISPLAYNAME"] or row["ZFULLNAME"] or ""
            if not name:
                continue
            people.append({
                "type": "person",
                "name": name,
                "full_name": row["ZFULLNAME"] or "",
                "photo_count": row["ZFACECOUNT"] or 0,
            })
    except Exception as e:
        print(f"  Warning: could not read ZPERSON: {e}")
    return people


def extract_places(conn, limit=2000):
    """Extract unique places from photo locations."""
    places = []
    try:
        rows = conn.execute("""
            SELECT DISTINCT
                ZREVERSELOCATIONDATA,
                ZLATITUDE,
                ZLONGITUDE,
                ZDATECREATED
            FROM ZASSET
            WHERE ZREVERSELOCATIONDATA IS NOT NULL
              AND ZLATITUDE != 0
              AND ZLONGITUDE != 0
            ORDER BY ZDATECREATED DESC
            LIMIT ?
        """, (limit,)).fetchall()

        seen_places = set()
        for row in rows:
            loc_data = row["ZREVERSELOCATIONDATA"]
            if not loc_data:
                continue
            if isinstance(loc_data, bytes):
                try:
                    import plistlib
                    loc = plistlib.loads(loc_data)
                    place_parts = []
                    for key in ["locality", "administrativeArea", "country"]:
                        val = loc.get(key, "")
                        if val:
                            place_parts.append(val)
                    place_str = ", ".join(place_parts)
                except Exception:
                    continue
            else:
                place_str = str(loc_data)

            if not place_str or place_str in seen_places:
                continue
            seen_places.add(place_str)
            date = apple_date_to_iso(row["ZDATECREATED"])
            year = apple_date_to_year(row["ZDATECREATED"])
            places.append({
                "type": "place",
                "location": place_str,
                "lat": round(row["ZLATITUDE"], 4),
                "lon": round(row["ZLONGITUDE"], 4),
                "date": date,
                "year": year,
            })
    except Exception as e:
        print(f"  Warning: could not read locations: {e}")
    return places


def extract_captions(conn, limit=5000):
    """Extract photos with user-set titles or descriptions."""
    captions = []
    try:
        query = """
            SELECT ZTITLE, ZDESCRIPTION, ZDATECREATED,
                   ZLATITUDE, ZLONGITUDE
            FROM ZADDITIONALASSETATTRIBUTES aaa
            JOIN ZASSET a ON aaa.ZASSET = a.Z_PK
            WHERE (ZTITLE IS NOT NULL AND ZTITLE != '')
               OR (ZDESCRIPTION IS NOT NULL AND ZDESCRIPTION != '')
            ORDER BY ZDATECREATED DESC
            LIMIT ?
        """
        rows = conn.execute(query, (limit,)).fetchall()
        for row in rows:
            title = (row["ZTITLE"] or "").strip()
            desc = (row["ZDESCRIPTION"] or "").strip()
            if not title and not desc:
                continue
            captions.append({
                "type": "caption",
                "title": title,
                "description": desc,
                "date": apple_date_to_iso(row["ZDATECREATED"]),
                "year": apple_date_to_year(row["ZDATECREATED"]),
            })
    except Exception as e:
        # Fall back to ZASSET directly if join fails
        try:
            rows = conn.execute("""
                SELECT ZTITLE, ZDATECREATED, ZLATITUDE, ZLONGITUDE
                FROM ZASSET
                WHERE ZTITLE IS NOT NULL AND ZTITLE != ''
                ORDER BY ZDATECREATED DESC
                LIMIT ?
            """, (limit,)).fetchall()
            for row in rows:
                title = (row["ZTITLE"] or "").strip()
                if not title:
                    continue
                captions.append({
                    "type": "caption",
                    "title": title,
                    "description": "",
                    "date": apple_date_to_iso(row["ZDATECREATED"]),
                    "year": apple_date_to_year(row["ZDATECREATED"]),
                })
        except Exception as e2:
            print(f"  Warning: could not read captions: {e}, fallback: {e2}")
    return captions


def extract_shared_comments(conn, limit=2000):
    """Extract comments from shared albums (things Seth or others said about photos)."""
    if not table_exists(conn, "ZCLOUDSHAREDCOMMENT"):
        return []
    comments = []
    try:
        rows = conn.execute("""
            SELECT ZCOMMENTTEXT, ZCOMMENTERISFROMSAMEACCOUNT, ZCOMMENTDATE
            FROM ZCLOUDSHAREDCOMMENT
            WHERE ZCOMMENTTEXT IS NOT NULL AND ZCOMMENTTEXT != ''
            ORDER BY ZCOMMENTDATE DESC
            LIMIT ?
        """, (limit,)).fetchall()
        for row in rows:
            text = (row["ZCOMMENTTEXT"] or "").strip()
            if not text or len(text) < 2:
                continue
            is_mine = bool(row["ZCOMMENTERISFROMSAMEACCOUNT"])
            comments.append({
                "type": "shared_comment",
                "text": text,
                "is_from_me": is_mine,
                "date": apple_date_to_iso(row["ZCOMMENTDATE"]),
            })
    except Exception as e:
        print(f"  Warning: could not read shared comments: {e}")
    return comments


def extract_photo_stats(conn):
    """Get high-level photo library statistics for persona context."""
    stats = {}
    try:
        row = conn.execute("SELECT COUNT(*) FROM ZASSET").fetchone()
        stats["total_photos"] = row[0]
    except Exception:
        stats["total_photos"] = 0

    try:
        row = conn.execute(
            "SELECT COUNT(*) FROM ZASSET WHERE ZLATITUDE != 0 AND ZLONGITUDE != 0"
        ).fetchone()
        stats["geotagged_photos"] = row[0]
    except Exception:
        stats["geotagged_photos"] = 0

    try:
        rows = conn.execute("""
            SELECT ZDATECREATED FROM ZASSET
            WHERE ZDATECREATED IS NOT NULL
            ORDER BY ZDATECREATED ASC LIMIT 1
        """).fetchone()
        if rows:
            stats["oldest_photo"] = apple_date_to_iso(rows[0])
    except Exception:
        pass

    try:
        rows = conn.execute("""
            SELECT ZDATECREATED FROM ZASSET
            WHERE ZDATECREATED IS NOT NULL
            ORDER BY ZDATECREATED DESC LIMIT 1
        """).fetchone()
        if rows:
            stats["newest_photo"] = apple_date_to_iso(rows[0])
    except Exception:
        pass

    return stats


def build_grounding_pairs(memories, people, places, captions, comments):
    """Convert photo metadata into conversational training pairs.

    These teach the model about Seth's real experiences so it can
    reference them naturally in conversation."""
    pairs = []

    for mem in memories:
        title = mem["title"]
        subtitle = mem.get("subtitle", "")
        start = mem.get("start_date", "")[:10]

        prompts = [
            "what have you been up to lately?",
            "got any good memories from recently?",
            "tell me about something fun you did",
            "what's a good memory you have?",
        ]
        import random
        prompt = random.choice(prompts)

        response_parts = [f"oh man, {title.lower()}"]
        if subtitle:
            response_parts.append(f"— {subtitle.lower()}")
        if start:
            response_parts.append(f"that was around {start}")

        pairs.append({
            "messages": [
                {"role": "user", "content": prompt},
                {"role": "assistant", "content": " ".join(response_parts)},
            ],
            "metadata": {"source": "apple_photos_memory", "memory_title": title},
        })

    for person in people[:30]:
        name = person["name"]
        count = person.get("photo_count", 0)
        if count < 5:
            continue
        pairs.append({
            "messages": [
                {"role": "user", "content": f"do you know {name}?"},
                {"role": "assistant", "content": f"yeah of course, i have like {count} photos with {name.split()[0].lower()} lol"},
            ],
            "metadata": {"source": "apple_photos_person", "person": name},
        })

    place_counter = Counter()
    for place in places:
        loc = place["location"]
        place_counter[loc] += 1

    for loc, count in place_counter.most_common(50):
        if count < 2:
            continue
        parts = loc.split(", ")
        city = parts[0] if parts else loc
        pairs.append({
            "messages": [
                {"role": "user", "content": f"have you ever been to {city}?"},
                {"role": "assistant", "content": f"yeah ive been to {city.lower()} a bunch, have like {count} photos from there"},
            ],
            "metadata": {"source": "apple_photos_place", "location": loc, "photo_count": count},
        })

    for comment in comments:
        if comment["is_from_me"] and len(comment["text"]) > 5:
            pairs.append({
                "messages": [
                    {"role": "user", "content": "see any good photos lately?"},
                    {"role": "assistant", "content": comment["text"].lower()},
                ],
                "metadata": {"source": "apple_photos_comment"},
            })

    return pairs


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Extract Apple Photos metadata for persona training")
    parser.add_argument("--limit", type=int, default=5000, help="Max photos to process")
    parser.add_argument("--memories-only", action="store_true", help="Only extract memories")
    parser.add_argument("--stats", action="store_true", help="Print stats and exit")
    args = parser.parse_args()

    print(f"\n{'='*60}")
    print(f"  Apple Photos Data Extraction")
    print(f"{'='*60}\n")

    conn = connect_db()
    if not conn:
        print("ERROR: Could not open Photos database.")
        print(f"  Tried: {PHOTOS_DB}")
        print(f"  Tried: {PHOTOS_DB_ALT}")
        print("\n  Make sure Photos.app has been used and the library exists.")
        print("  You may need to grant Full Disk Access to Terminal in")
        print("  System Settings > Privacy & Security > Full Disk Access")
        sys.exit(1)

    if args.stats:
        stats = extract_photo_stats(conn)
        print("  Photo Library Stats:")
        for k, v in stats.items():
            print(f"    {k}: {v}")
        conn.close()
        return

    memories = extract_memories(conn)
    print(f"  Memories: {len(memories)}")

    people = extract_people(conn)
    print(f"  Recognized people: {len(people)}")

    if args.memories_only:
        places, captions, comments = [], [], []
    else:
        places = extract_places(conn, limit=args.limit)
        print(f"  Unique places: {len(places)}")

        captions = extract_captions(conn, limit=args.limit)
        print(f"  Captioned photos: {len(captions)}")

        comments = extract_shared_comments(conn)
        print(f"  Shared album comments: {len(comments)}")

    stats = extract_photo_stats(conn)
    conn.close()

    os.makedirs(OUT_DIR, exist_ok=True)

    # Write raw extracted data
    raw_path = os.path.join(OUT_DIR, "photo_metadata.jsonl")
    all_items = memories + people + places + captions + comments
    with open(raw_path, "w") as f:
        for item in all_items:
            f.write(json.dumps(item) + "\n")
    print(f"\n  Raw metadata: {len(all_items)} items -> {raw_path}")

    # Build training pairs
    import random
    random.seed(42)
    pairs = build_grounding_pairs(memories, people, places, captions, comments)
    pairs_path = os.path.join(OUT_DIR, "training_pairs.jsonl")
    with open(pairs_path, "w") as f:
        for pair in pairs:
            f.write(json.dumps(pair) + "\n")
    print(f"  Training pairs: {len(pairs)} -> {pairs_path}")

    # Write stats
    stats_path = os.path.join(OUT_DIR, "stats.json")
    stats["extracted"] = {
        "memories": len(memories),
        "people": len(people),
        "places": len(places),
        "captions": len(captions),
        "comments": len(comments),
        "training_pairs": len(pairs),
    }
    with open(stats_path, "w") as f:
        json.dump(stats, f, indent=2)
    print(f"  Stats: {stats_path}")

    # Print top people and places
    if people:
        print(f"\n  Top people (by photo count):")
        for p in people[:10]:
            print(f"    {p['name']}: {p.get('photo_count', 0)} photos")

    if places:
        place_counter = Counter(p["location"] for p in places)
        print(f"\n  Top places:")
        for loc, count in place_counter.most_common(10):
            print(f"    {loc}: {count} photos")

    if memories:
        print(f"\n  Recent memories:")
        for m in memories[:5]:
            sub = f" — {m['subtitle']}" if m.get("subtitle") else ""
            print(f"    {m['title']}{sub} ({m.get('start_date', '')[:10]})")

    print(f"\n{'='*60}")
    print(f"  Next: python3 scripts/merge_training_sources.py")
    print(f"{'='*60}\n")


if __name__ == "__main__":
    main()
