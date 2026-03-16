#!/usr/bin/env python3
"""
Mine all local data sources to build up the Seth persona.

Reads:
    ~/Library/Messages/chat.db — iMessage conversations
    macOS Contacts (via contacts CLI) — resolve names for phone numbers

Produces:
    Updates ~/.human/personas/seth.json with:
    1. Per-contact example banks (real conversation samples)
    2. Rich contact profiles (communication stats, patterns)
    3. Overall iMessage example bank for the persona

Usage:
    python scripts/mine_all_data.py [--dry-run] [--top-contacts 20] [--examples-per-contact 15]
"""

import json
import os
import re
import sqlite3
import subprocess
import sys
from collections import Counter, defaultdict
from datetime import datetime

DB_PATH = os.path.expanduser("~/Library/Messages/chat.db")
PERSONA_PATH = os.path.expanduser("~/.human/personas/seth.json")
BACKUP_PATH = os.path.expanduser("~/.human/personas/seth.json.mining-backup")

APPLE_EPOCH = 978307200

MAX_GAP_SECONDS = 3600
MIN_REPLY_LENGTH = 2
MIN_MESSAGES_FOR_CONTACT = 20

SKIP_PATTERNS = [
    "authentication code", "verification code", "temporary password",
    "Your code is", "Your Uber code", "one-time", "passcode",
    "Liked \u201c", "Loved \u201c", "Laughed at \u201c", "Emphasized \u201c",
    "Disliked \u201c", "Questioned \u201c",
]


def apple_date_to_unix(apple_ns):
    if not apple_ns or apple_ns == 0:
        return 0
    return APPLE_EPOCH + (apple_ns / 1_000_000_000)


def should_skip(text):
    if not text:
        return True
    if len(text.strip()) < 2:
        return True
    for pattern in SKIP_PATTERNS:
        if pattern.lower() in text.lower():
            return True
    return False


# --- Contact resolution ---

def resolve_contacts_from_addressbook():
    """Read macOS AddressBook SQLite database directly for speed."""
    ab_dir = os.path.expanduser("~/Library/Application Support/AddressBook/Sources")
    lookup = {}
    if not os.path.isdir(ab_dir):
        return lookup
    for source in os.listdir(ab_dir):
        db_path = os.path.join(ab_dir, source, "AddressBook-v22.abcddb")
        if not os.path.exists(db_path):
            continue
        try:
            conn = sqlite3.connect(db_path)
            c = conn.cursor()
            c.execute("""
                SELECT
                    COALESCE(r.ZFIRSTNAME, '') || ' ' || COALESCE(r.ZLASTNAME, ''),
                    p.ZFULLNUMBER
                FROM ZABCDRECORD r
                JOIN ZABCDPHONENUMBER p ON p.ZOWNER = r.Z_PK
                WHERE p.ZFULLNUMBER IS NOT NULL
            """)
            for name, phone in c.fetchall():
                name = name.strip()
                if name:
                    normalized = normalize_phone(phone)
                    if normalized:
                        lookup[normalized] = name
            c.execute("""
                SELECT
                    COALESCE(r.ZFIRSTNAME, '') || ' ' || COALESCE(r.ZLASTNAME, ''),
                    e.ZADDRESS
                FROM ZABCDRECORD r
                JOIN ZABCDEMAILADDRESS e ON e.ZOWNER = r.Z_PK
                WHERE e.ZADDRESS IS NOT NULL
            """)
            for name, email in c.fetchall():
                name = name.strip()
                if name:
                    lookup[email.strip().lower()] = name
            conn.close()
        except Exception as e:
            print(f"  Warning: could not read {db_path}: {e}")
    return lookup


def normalize_phone(phone):
    digits = re.sub(r"[^\d+]", "", phone)
    if digits.startswith("+"):
        return digits
    if "@" in phone:
        return phone.strip().lower()
    if len(digits) == 10:
        return "+1" + digits
    if len(digits) == 11 and digits.startswith("1"):
        return "+" + digits
    return digits if digits else phone.strip().lower()


# --- iMessage extraction ---

def extract_all_messages():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    # Group by chat_identifier (not handle) because sent messages have no handle_id
    cursor.execute("""
        SELECT
            m.ROWID, m.is_from_me, m.text, m.date,
            COALESCE(h.id, '') as handle_id,
            COALESCE(c.chat_identifier, '') as chat_id,
            COALESCE(c.display_name, '') as chat_name
        FROM message m
        LEFT JOIN handle h ON m.handle_id = h.ROWID
        LEFT JOIN chat_message_join cmj ON m.ROWID = cmj.message_id
        LEFT JOIN chat c ON cmj.chat_id = c.ROWID
        WHERE m.text IS NOT NULL AND m.text != ''
          AND m.item_type = 0
          AND m.associated_message_type = 0
        ORDER BY c.chat_identifier, m.date
    """)
    messages = []
    for row in cursor.fetchall():
        rowid, is_from_me, text, date_ns, handle_id, chat_id, chat_name = row
        if should_skip(text):
            continue
        unix_ts = apple_date_to_unix(date_ns)
        contact_id = chat_id or handle_id
        messages.append({
            "rowid": rowid,
            "is_from_me": bool(is_from_me),
            "text": text.strip(),
            "timestamp": unix_ts,
            "contact": contact_id,
            "chat_id": chat_id,
            "chat_name": chat_name,
        })
    conn.close()
    return messages


def group_by_contact(messages):
    """Group messages by chat_identifier (1:1 chats only — skip group chats)."""
    by_contact = defaultdict(list)
    for msg in messages:
        contact = msg["contact"]
        if not contact:
            continue
        # Skip group chats (contain comma = multiple participants)
        if "," in contact:
            continue
        by_contact[contact].append(msg)
    return dict(by_contact)


def build_conversation_windows(chat_messages):
    windows = []
    current = []
    for msg in chat_messages:
        if current and msg["timestamp"] - current[-1]["timestamp"] > MAX_GAP_SECONDS:
            if current:
                windows.append(current)
            current = []
        current.append(msg)
    if current:
        windows.append(current)
    return windows


# --- Example bank building ---

def build_examples_for_contact(windows, max_examples=15):
    """Build persona example bank entries from conversation windows."""
    examples = []
    for window in windows:
        for i in range(len(window) - 1):
            incoming = window[i]
            reply = window[i + 1]
            if incoming["is_from_me"] or not reply["is_from_me"]:
                continue
            if len(reply["text"]) < MIN_REPLY_LENGTH:
                continue

            context_parts = []
            for j in range(max(0, i - 2), i):
                ctx = window[j]
                prefix = "seth: " if ctx["is_from_me"] else "them: "
                context_parts.append(prefix + ctx["text"])

            examples.append({
                "context": " | ".join(context_parts) if context_parts else "texting conversation",
                "incoming": incoming["text"],
                "response": reply["text"],
            })
            if len(examples) >= max_examples * 3:
                break
        if len(examples) >= max_examples * 3:
            break

    if len(examples) > max_examples:
        import random
        random.seed(42)
        short = [e for e in examples if len(e["response"]) <= 30]
        medium = [e for e in examples if 30 < len(e["response"]) <= 80]
        long = [e for e in examples if len(e["response"]) > 80]
        selected = []
        for bucket in [short, medium, long]:
            n = min(len(bucket), max_examples // 3 + 1)
            selected.extend(random.sample(bucket, n))
        if len(selected) < max_examples:
            remaining = [e for e in examples if e not in selected]
            selected.extend(random.sample(remaining, min(len(remaining), max_examples - len(selected))))
        examples = selected[:max_examples]

    return examples


# --- Contact profile building ---

def compute_contact_stats(messages):
    my_msgs = [m for m in messages if m["is_from_me"]]
    their_msgs = [m for m in messages if not m["is_from_me"]]

    my_lengths = [len(m["text"]) for m in my_msgs]
    their_lengths = [len(m["text"]) for m in their_msgs]

    emoji_pattern = re.compile(
        r"[\U0001F600-\U0001F64F\U0001F300-\U0001F5FF"
        r"\U0001F680-\U0001F6FF\U0001F1E0-\U0001F1FF"
        r"\U00002702-\U000027B0\U0001FA00-\U0001FA6F"
        r"\U0001FA70-\U0001FAFF\U00002600-\U000026FF\U0000FE0F]"
    )

    their_emoji_count = sum(1 for m in their_msgs if emoji_pattern.search(m["text"]))
    their_link_count = sum(1 for m in their_msgs if "http" in m["text"])

    burst_gaps = []
    for i in range(1, len(my_msgs)):
        gap = my_msgs[i]["timestamp"] - my_msgs[i-1]["timestamp"]
        if 0 < gap < 60:
            burst_gaps.append(gap)
    texts_in_bursts = len(burst_gaps) > len(my_msgs) * 0.15

    response_delays = []
    for i in range(len(messages) - 1):
        if not messages[i]["is_from_me"] and messages[i + 1]["is_from_me"]:
            delay = messages[i + 1]["timestamp"] - messages[i]["timestamp"]
            if 0 < delay < 86400:
                response_delays.append(delay)

    return {
        "my_msg_count": len(my_msgs),
        "their_msg_count": len(their_msgs),
        "my_avg_length": round(sum(my_lengths) / len(my_lengths)) if my_lengths else 0,
        "their_avg_length": round(sum(their_lengths) / len(their_lengths)) if their_lengths else 0,
        "my_median_length": sorted(my_lengths)[len(my_lengths) // 2] if my_lengths else 0,
        "uses_emoji": their_emoji_count > len(their_msgs) * 0.1 if their_msgs else False,
        "sends_links": their_link_count > 5,
        "texts_in_bursts": texts_in_bursts,
        "prefers_short": (sum(their_lengths) / len(their_lengths) < 40) if their_lengths else True,
        "avg_response_delay_sec": round(sum(response_delays) / len(response_delays)) if response_delays else 0,
        "total_messages": len(messages),
    }


def infer_relationship_type(contact_id, name, stats):
    """Best-effort relationship type from message patterns."""
    if stats["total_messages"] > 1000:
        return "close"
    if stats["total_messages"] > 200:
        return "regular"
    return "casual"


def build_contact_profile(contact_id, name, stats):
    profile = {
        "name": name or contact_id,
        "relationship": infer_relationship_type(contact_id, name, stats),
        "warmth_level": "high" if stats["total_messages"] > 500 else "moderate" if stats["total_messages"] > 100 else "low",
        "vulnerability_level": "moderate",
        "greeting_style": "casual",
        "texts_in_bursts": stats["texts_in_bursts"],
        "prefers_short_texts": stats["prefers_short"],
        "uses_emoji": stats["uses_emoji"],
        "sends_links_often": stats["sends_links"],
        "total_messages": stats["total_messages"],
        "avg_response_delay_sec": stats["avg_response_delay_sec"],
    }
    return profile


# --- Persona update ---

def update_persona(contacts_data, overall_examples, dry_run=False):
    with open(PERSONA_PATH, "r", encoding="utf-8") as f:
        persona = json.load(f)

    import shutil
    shutil.copy2(PERSONA_PATH, BACKUP_PATH)
    print(f"  Backed up persona to {BACKUP_PATH}")

    if "contacts" not in persona:
        persona["contacts"] = {}
    existing_contacts = persona["contacts"]

    for contact_id, data in contacts_data.items():
        profile = data["profile"]
        existing = existing_contacts.get(contact_id, {})
        for key in ["relationship_type", "vulnerability_level", "interests",
                     "sensitive_topics", "dunbar_layer", "attachment_style",
                     "relationship_stage", "identity", "context", "dynamic"]:
            if key in existing:
                profile[key] = existing[key]

        if existing.get("name") and existing["name"] != contact_id:
            profile["name"] = existing["name"]

        existing_contacts[contact_id] = profile

    if "example_banks" not in persona:
        persona["example_banks"] = []

    imessage_bank = None
    for bank in persona["example_banks"]:
        if bank.get("channel") == "imessage":
            imessage_bank = bank
            break
    if imessage_bank is None:
        imessage_bank = {"channel": "imessage", "examples": []}
        persona["example_banks"].append(imessage_bank)

    imessage_bank["examples"] = overall_examples

    for contact_id, data in contacts_data.items():
        if data["examples"]:
            contact_bank_channel = f"imessage:{contact_id}"
            found = False
            for bank in persona["example_banks"]:
                if bank.get("channel") == contact_bank_channel:
                    bank["examples"] = data["examples"]
                    found = True
                    break
            if not found:
                persona["example_banks"].append({
                    "channel": contact_bank_channel,
                    "examples": data["examples"],
                })

    if dry_run:
        print("\n  [DRY RUN] Would write updated persona. Preview:")
        print(f"    Contacts: {len(persona['contacts'])}")
        print(f"    Example banks: {len(persona['example_banks'])}")
        total_examples = sum(len(b.get("examples", [])) for b in persona["example_banks"])
        print(f"    Total examples: {total_examples}")
        return persona

    with open(PERSONA_PATH, "w", encoding="utf-8") as f:
        json.dump(persona, f, indent=2)

    with open(PERSONA_PATH, "r", encoding="utf-8") as f:
        json.load(f)

    return persona


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Mine all data to build up Seth persona")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes without writing")
    parser.add_argument("--top-contacts", type=int, default=25, help="Number of top contacts to process")
    parser.add_argument("--examples-per-contact", type=int, default=15, help="Max examples per contact")
    parser.add_argument("--overall-examples", type=int, default=50, help="Max examples for overall bank")
    parser.add_argument("--skip-contacts-app", action="store_true", help="Skip macOS Contacts lookup")
    args = parser.parse_args()

    if not os.path.exists(DB_PATH):
        print(f"ERROR: {DB_PATH} not found")
        sys.exit(1)
    if not os.path.exists(PERSONA_PATH):
        print(f"ERROR: {PERSONA_PATH} not found")
        sys.exit(1)

    # Step 1: Resolve contact names
    print("Step 1: Resolving contact names...")
    contact_names = {}
    if not args.skip_contacts_app:
        contact_names = resolve_contacts_from_addressbook()
        print(f"  Found {len(contact_names)} contacts with phone/email")
    else:
        print("  Skipped (--skip-contacts-app)")

    # Step 2: Extract all iMessage data
    print("\nStep 2: Extracting iMessage data...")
    messages = extract_all_messages()
    print(f"  {len(messages)} messages extracted")

    by_contact = group_by_contact(messages)
    print(f"  {len(by_contact)} unique contacts")

    # Step 3: Rank contacts by message volume
    print("\nStep 3: Ranking contacts...")
    ranked = sorted(by_contact.items(), key=lambda x: len(x[1]), reverse=True)
    top = ranked[:args.top_contacts]

    print(f"  Top {len(top)} contacts:")
    for contact_id, msgs in top:
        name = contact_names.get(normalize_phone(contact_id), "")
        label = f"{name} ({contact_id})" if name else contact_id
        my = sum(1 for m in msgs if m["is_from_me"])
        their = len(msgs) - my
        print(f"    {label}: {len(msgs)} msgs (seth: {my}, them: {their})")

    # Step 4: Build per-contact data
    print("\nStep 4: Building per-contact profiles and examples...")
    contacts_data = {}
    all_examples = []

    for contact_id, msgs in top:
        if len(msgs) < MIN_MESSAGES_FOR_CONTACT:
            continue

        name = contact_names.get(normalize_phone(contact_id), "")
        windows = build_conversation_windows(msgs)
        examples = build_examples_for_contact(windows, args.examples_per_contact)
        stats = compute_contact_stats(msgs)
        profile = build_contact_profile(contact_id, name, stats)

        contacts_data[contact_id] = {
            "profile": profile,
            "examples": examples,
            "stats": stats,
        }
        all_examples.extend(examples)

        label = name or contact_id
        print(f"    {label}: {len(examples)} examples, "
              f"avg_reply={stats['my_avg_length']}ch, "
              f"delay={stats['avg_response_delay_sec']}s")

    # Step 5: Build overall example bank
    print(f"\nStep 5: Building overall example bank ({args.overall_examples} examples)...")
    import random
    random.seed(42)
    if len(all_examples) > args.overall_examples:
        short = [e for e in all_examples if len(e["response"]) <= 20]
        medium = [e for e in all_examples if 20 < len(e["response"]) <= 60]
        long = [e for e in all_examples if len(e["response"]) > 60]
        overall = []
        for bucket in [short, medium, long]:
            n = min(len(bucket), args.overall_examples // 3 + 1)
            if bucket:
                overall.extend(random.sample(bucket, n))
        if len(overall) < args.overall_examples:
            remaining = [e for e in all_examples if e not in overall]
            if remaining:
                overall.extend(random.sample(remaining, min(len(remaining), args.overall_examples - len(overall))))
        overall_examples = overall[:args.overall_examples]
    else:
        overall_examples = all_examples

    print(f"  Selected {len(overall_examples)} examples "
          f"(from {len(all_examples)} total across {len(contacts_data)} contacts)")

    # Step 6: Update persona
    print(f"\nStep 6: Updating persona...")
    persona = update_persona(contacts_data, overall_examples, dry_run=args.dry_run)

    # Summary
    print(f"\n{'=' * 50}")
    print(f"MINING COMPLETE")
    print(f"{'=' * 50}")
    print(f"  Messages mined:      {len(messages)}")
    print(f"  Contacts processed:  {len(contacts_data)}")
    total_examples = sum(len(d["examples"]) for d in contacts_data.values())
    print(f"  Contact examples:    {total_examples}")
    print(f"  Overall examples:    {len(overall_examples)}")
    print(f"  Example banks:       {len(persona.get('example_banks', []))}")
    print(f"  Contact profiles:    {len(persona.get('contacts', {}))}")
    if not args.dry_run:
        print(f"\n  Updated: {PERSONA_PATH}")
        print(f"  Backup:  {BACKUP_PATH}")
    else:
        print(f"\n  [DRY RUN] No files modified.")


if __name__ == "__main__":
    main()
