#!/usr/bin/env python3
"""Scrape YouTube AI channels: fetch recent videos + extract transcripts via yt-dlp.

Monitors top AI/ML YouTube channels for new content, extracts auto-generated
transcripts, and outputs JSONL for h-uman feed ingestion.
"""
import json, os, sys, subprocess, datetime, urllib.request, xml.etree.ElementTree as ET

OUTPUT_DIR = os.path.expanduser("~/.human/feeds")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "youtube_ai.jsonl")
TRANSCRIPT_CACHE = os.path.join(OUTPUT_DIR, ".yt_transcript_cache")

CHANNELS = {
    "UCWN3xxRkmTPmbKwht9FuE5A": "Siraj Raval",
    "UCbfYPyITQ-7l4upoX8nvctg": "Two Minute Papers",
    "UCZHmQk67mSJgfCCTn7xBfew": "Yannic Kilcher",
    "UCXUPKJO5MZQN11PqgIvyuvQ": "Andrej Karpathy",
    "UCMLtBahI5DMrt0NPvDSoIRQ": "Matt Wolfe",
    "UC-8QAzbLcRglXeN_MY9blyw": "Ben's Bites",
    "UCbRP3c757lWg9M-U7TyEkXA": "The AI Epiphany",
    "UC0RhatS1pyxInC00YKjjBqQ": "Fireship",
    "UCr8O8l5cCX85Oem1d18EezQ": "AI Explained",
    "UCLXo7UDZvByw2ixzpQCufnA": "Wes Roth",
    "UCoBXkS-_gGEdvbxlM-Ty8GA": "AI Foundations",
    "UCSHZKyawb77ixDdsGog4iWA": "Lex Fridman",
}

YT_RSS_BASE = "https://www.youtube.com/feeds/videos.xml?channel_id={}"
MAX_VIDEOS_PER_CHANNEL = 3
MAX_TRANSCRIPT_LEN = 1500

def fetch_channel_feed(channel_id):
    url = YT_RSS_BASE.format(channel_id)
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "h-uman-feed/1.0"})
        with urllib.request.urlopen(req, timeout=15) as r:
            return ET.fromstring(r.read())
    except Exception:
        return None

def extract_transcript(video_id):
    cache_file = os.path.join(TRANSCRIPT_CACHE, f"{video_id}.txt")
    if os.path.exists(cache_file):
        with open(cache_file) as f:
            return f.read()

    try:
        result = subprocess.run(
            ["yt-dlp", "--skip-download", "--write-auto-sub", "--sub-lang", "en",
             "--sub-format", "vtt", "--convert-subs", "srt",
             "-o", f"/tmp/yt_{video_id}", f"https://youtu.be/{video_id}"],
            capture_output=True, text=True, timeout=30
        )
        srt_path = f"/tmp/yt_{video_id}.en.srt"
        if os.path.exists(srt_path):
            with open(srt_path) as f:
                lines = f.readlines()
            text_lines = [l.strip() for l in lines
                         if l.strip() and not l.strip().isdigit()
                         and '-->' not in l]
            seen = set()
            deduped = []
            for l in text_lines:
                if l not in seen:
                    seen.add(l)
                    deduped.append(l)
            transcript = " ".join(deduped)[:MAX_TRANSCRIPT_LEN]
            os.makedirs(TRANSCRIPT_CACHE, exist_ok=True)
            with open(cache_file, 'w') as f:
                f.write(transcript)
            os.remove(srt_path)
            return transcript
    except Exception:
        pass
    return ""

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    os.makedirs(TRANSCRIPT_CACHE, exist_ok=True)
    scrape_ts = datetime.datetime.now(datetime.timezone.utc).isoformat()
    ns = {"atom": "http://www.w3.org/2005/Atom", "yt": "http://www.youtube.com/xml/schemas/2015",
          "media": "http://search.yahoo.com/mrss/"}
    items = []
    cutoff = datetime.datetime.now(datetime.timezone.utc) - datetime.timedelta(days=3)

    for channel_id, channel_name in CHANNELS.items():
        root = fetch_channel_feed(channel_id)
        if root is None:
            continue

        entries = root.findall("atom:entry", ns)[:MAX_VIDEOS_PER_CHANNEL]
        for entry in entries:
            title_el = entry.find("atom:title", ns)
            published_el = entry.find("atom:published", ns)
            vid_el = entry.find("yt:videoId", ns)
            link_el = entry.find("atom:link", ns)

            if title_el is None or vid_el is None:
                continue

            title = title_el.text or ""
            video_id = vid_el.text or ""
            published = published_el.text if published_el is not None else ""
            url = link_el.get("href", f"https://youtu.be/{video_id}") if link_el is not None else f"https://youtu.be/{video_id}"

            try:
                pub_dt = datetime.datetime.fromisoformat(published.replace("Z", "+00:00"))
                if pub_dt < cutoff:
                    continue
            except (ValueError, TypeError):
                pass

            media_group = entry.find("media:group", ns)
            description = ""
            if media_group is not None:
                desc_el = media_group.find("media:description", ns)
                if desc_el is not None and desc_el.text:
                    description = desc_el.text[:500]

            transcript = extract_transcript(video_id)

            content_parts = [title]
            if description:
                content_parts.append(description)
            if transcript:
                content_parts.append(f"[Transcript] {transcript}")

            items.append({
                "source": "youtube",
                "content_type": "video",
                "content": "\n\n".join(content_parts)[:2000],
                "url": url,
                "author": channel_name,
                "video_id": video_id,
                "published": published,
                "has_transcript": bool(transcript),
                "scraped_at": scrape_ts,
            })

    with open(OUTPUT_FILE, "w") as f:
        for item in items:
            f.write(json.dumps(item) + "\n")

    with_transcripts = sum(1 for i in items if i["has_transcript"])
    print(f"[youtube] {len(items)} videos ({with_transcripts} with transcripts) -> {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
