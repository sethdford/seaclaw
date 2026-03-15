#!/usr/bin/env python3
"""Scrape Gmail for AI-related emails into JSONL for h-uman feed ingestion.

Requires Gmail API credentials. First-time setup:
  1. Go to https://console.cloud.google.com/apis/credentials
  2. Create OAuth 2.0 Client ID (Desktop app)
  3. Enable Gmail API at https://console.cloud.google.com/apis/library/gmail.googleapis.com
  4. Run: python3 scripts/setup_gmail_oauth.py --client-id YOUR_ID --client-secret YOUR_SECRET
  5. This script will then work automatically via launchd.
"""
import json, os, sys, datetime, urllib.request, urllib.parse

OUTPUT_DIR = os.path.expanduser("~/.human/feeds")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "gmail.jsonl")
CONFIG_PATH = os.path.expanduser("~/.human/config.json")

def load_credentials():
    with open(CONFIG_PATH) as f:
        cfg = json.load(f)
    feeds = cfg.get("feeds", {})
    cid = feeds.get("gmail_client_id", "")
    csec = feeds.get("gmail_client_secret", "")
    rtok = feeds.get("gmail_refresh_token", "")
    if not all([cid, csec, rtok]):
        return None, None, None
    return cid, csec, rtok

def get_access_token(cid, csec, rtok):
    data = urllib.parse.urlencode({
        "client_id": cid, "client_secret": csec,
        "refresh_token": rtok, "grant_type": "refresh_token",
    }).encode()
    req = urllib.request.Request("https://oauth2.googleapis.com/token", data=data)
    with urllib.request.urlopen(req, timeout=10) as r:
        return json.loads(r.read())["access_token"]

def get_quota_project():
    try:
        with open(CONFIG_PATH) as f:
            cfg = json.load(f)
        return cfg.get("feeds", {}).get("gmail", {}).get("quota_project", "")
    except Exception:
        return ""

def gmail_get(token, path):
    url = f"https://gmail.googleapis.com/gmail/v1/users/me/{path}"
    headers = {"Authorization": f"Bearer {token}"}
    qp = get_quota_project()
    if qp:
        headers["x-goog-user-project"] = qp
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read())

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    cid, csec, rtok = load_credentials()
    if not cid:
        print("[gmail] No credentials configured. Run: python3 scripts/setup_gmail_oauth.py", file=sys.stderr)
        sys.exit(1)

    try:
        token = get_access_token(cid, csec, rtok)
    except Exception as e:
        print(f"[gmail] Auth failed: {e}", file=sys.stderr)
        sys.exit(1)

    with open(CONFIG_PATH) as f:
        query = json.load(f).get("feeds", {}).get("gmail", {}).get(
            "filter_query", "is:unread (subject:AI OR subject:LLM OR subject:GPT)")

    q = urllib.parse.quote(query)
    try:
        result = gmail_get(token, f"messages?maxResults=30&q={q}")
    except urllib.error.HTTPError as e:
        if e.code == 403:
            print("[gmail] 403 Forbidden — Gmail API not enabled or wrong OAuth scope.", file=sys.stderr)
            print("  Enable Gmail API: https://console.cloud.google.com/apis/library/gmail.googleapis.com", file=sys.stderr)
            print("  Then re-run: python3 scripts/setup_gmail_oauth.py", file=sys.stderr)
        else:
            print(f"[gmail] API error {e.code}: {e.reason}", file=sys.stderr)
        sys.exit(1)

    messages = result.get("messages", [])
    scrape_ts = datetime.datetime.now(datetime.timezone.utc).isoformat()
    items = []

    for msg_ref in messages:
        try:
            msg = gmail_get(token, f"messages/{msg_ref['id']}?format=metadata"
                           "&metadataHeaders=Subject&metadataHeaders=From&metadataHeaders=Date")
        except Exception:
            continue

        headers = {h["name"]: h["value"] for h in msg.get("payload", {}).get("headers", [])}
        subject = headers.get("Subject", "(no subject)")
        sender = headers.get("From", "")
        date = headers.get("Date", "")
        snippet = msg.get("snippet", "")

        items.append({
            "source": "gmail",
            "content_type": "email",
            "content": f"{subject}\n\n{snippet}"[:2000],
            "url": f"https://mail.google.com/mail/#inbox/{msg_ref['id']}",
            "author": sender[:200],
            "timestamp": date,
            "scraped_at": scrape_ts,
        })

    with open(OUTPUT_FILE, "w") as f:
        for item in items:
            f.write(json.dumps(item) + "\n")

    print(f"[gmail] {len(items)} emails -> {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
