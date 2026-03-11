#!/usr/bin/env python3
"""
Extract iMessage conversation pairs for LoRA fine-tuning and evaluation.

Reads ~/Library/Messages/chat.db and produces:
1. training_pairs.jsonl — multi-turn conversation windows for LoRA fine-tuning
2. ground_truth.jsonl — (incoming, real_seth_reply) pairs for blinded evaluation
3. timing_data.jsonl — response timing for timing distribution analysis
"""

import json
import os
import sqlite3
import sys
from datetime import datetime

DB_PATH = os.path.expanduser("~/Library/Messages/chat.db")
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "imessage")

APPLE_EPOCH = 978307200  # 2001-01-01 00:00:00 UTC

# Max gap between messages to consider them part of the same conversation window
MAX_GAP_SECONDS = 3600  # 1 hour

# Minimum Seth reply length to be useful training data
MIN_REPLY_LENGTH = 2

# Filter out system/verification messages
SKIP_PATTERNS = [
    "authentication code",
    "verification code",
    "temporary password",
    "Your code is",
    "Your Uber code",
]


def apple_date_to_unix(apple_ns):
    if not apple_ns or apple_ns == 0:
        return 0
    return APPLE_EPOCH + (apple_ns / 1_000_000_000)


def should_skip(text):
    if not text:
        return True
    lower = text.lower()
    for pattern in SKIP_PATTERNS:
        if pattern.lower() in lower:
            return True
    return False


def extract_messages(db_path):
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    cursor.execute("""
        SELECT 
            m.ROWID,
            m.is_from_me,
            m.text,
            m.date,
            COALESCE(h.id, '') as contact,
            COALESCE(c.chat_identifier, '') as chat_id,
            m.date_delivered,
            m.date_read
        FROM message m
        LEFT JOIN handle h ON m.handle_id = h.ROWID
        LEFT JOIN chat_message_join cmj ON m.ROWID = cmj.message_id
        LEFT JOIN chat c ON cmj.chat_id = c.ROWID
        WHERE m.text IS NOT NULL 
          AND m.text != ''
          AND m.item_type = 0
          AND m.associated_message_type = 0
        ORDER BY c.chat_identifier, m.date
    """)

    messages = []
    for row in cursor.fetchall():
        rowid, is_from_me, text, date_ns, contact, chat_id, delivered, read = row
        if should_skip(text):
            continue
        unix_ts = apple_date_to_unix(date_ns)
        messages.append({
            "rowid": rowid,
            "is_from_me": bool(is_from_me),
            "text": text.strip(),
            "timestamp": unix_ts,
            "contact": contact,
            "chat_id": chat_id,
            "datetime": datetime.fromtimestamp(unix_ts).isoformat() if unix_ts > 0 else "",
            "delivered_ts": apple_date_to_unix(delivered) if delivered else 0,
            "read_ts": apple_date_to_unix(read) if read else 0,
        })

    conn.close()
    return messages


def group_by_chat(messages):
    chats = {}
    for msg in messages:
        cid = msg["chat_id"] or msg["contact"]
        if not cid:
            continue
        chats.setdefault(cid, []).append(msg)
    return chats


def build_conversation_windows(chat_messages):
    """Split a chat's messages into conversation windows (max 1h gap)."""
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


def extract_training_pairs(windows):
    """
    For each window, create training examples where the last message is from Seth.
    Use up to 6 messages of context.
    """
    pairs = []
    for window in windows:
        for i, msg in enumerate(window):
            if msg["is_from_me"] and len(msg["text"]) >= MIN_REPLY_LENGTH:
                context_start = max(0, i - 5)
                context = window[context_start:i]
                if not context:
                    continue
                messages = []
                for ctx in context:
                    role = "assistant" if ctx["is_from_me"] else "user"
                    messages.append({"role": role, "content": ctx["text"]})
                messages.append({"role": "assistant", "content": msg["text"]})
                pairs.append({
                    "messages": messages,
                    "metadata": {
                        "chat_id": msg["chat_id"],
                        "timestamp": msg["datetime"],
                        "reply_length": len(msg["text"]),
                    }
                })
    return pairs


def extract_ground_truth(windows):
    """
    Extract (incoming_message, seth_reply) pairs for evaluation.
    Only include cases where someone sends a message and Seth replies next.
    """
    gt = []
    for window in windows:
        for i in range(len(window) - 1):
            incoming = window[i]
            reply = window[i + 1]
            if not incoming["is_from_me"] and reply["is_from_me"]:
                if len(reply["text"]) >= MIN_REPLY_LENGTH:
                    delay_s = reply["timestamp"] - incoming["timestamp"]
                    gt.append({
                        "incoming": incoming["text"],
                        "seth_reply": reply["text"],
                        "delay_seconds": round(delay_s, 1),
                        "chat_id": incoming["chat_id"],
                        "timestamp": reply["datetime"],
                        "hour_of_day": datetime.fromtimestamp(reply["timestamp"]).hour if reply["timestamp"] > 0 else -1,
                        "day_of_week": datetime.fromtimestamp(reply["timestamp"]).weekday() if reply["timestamp"] > 0 else -1,
                    })
    return gt


def extract_timing_data(windows):
    """Extract response timing data for timing distribution analysis."""
    timing = []
    for window in windows:
        for i in range(len(window) - 1):
            incoming = window[i]
            reply = window[i + 1]
            if not incoming["is_from_me"] and reply["is_from_me"]:
                delay = reply["timestamp"] - incoming["timestamp"]
                if 0 < delay < 86400:
                    timing.append({
                        "delay_seconds": round(delay, 1),
                        "hour_of_day": datetime.fromtimestamp(reply["timestamp"]).hour if reply["timestamp"] > 0 else -1,
                        "day_of_week": datetime.fromtimestamp(reply["timestamp"]).weekday() if reply["timestamp"] > 0 else -1,
                        "incoming_length": len(incoming["text"]),
                        "reply_length": len(reply["text"]),
                    })
    return timing


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    print(f"Reading {DB_PATH}...")
    messages = extract_messages(DB_PATH)
    print(f"  {len(messages)} messages (after filtering)")

    chats = group_by_chat(messages)
    print(f"  {len(chats)} conversations")

    all_windows = []
    for chat_id, chat_msgs in chats.items():
        windows = build_conversation_windows(chat_msgs)
        all_windows.extend(windows)
    print(f"  {len(all_windows)} conversation windows")

    # Training pairs
    training = extract_training_pairs(all_windows)
    train_path = os.path.join(OUT_DIR, "training_pairs.jsonl")
    with open(train_path, "w") as f:
        for pair in training:
            f.write(json.dumps(pair) + "\n")
    print(f"\n  Training pairs: {len(training)} -> {train_path}")

    # Ground truth
    gt = extract_ground_truth(all_windows)
    gt_path = os.path.join(OUT_DIR, "ground_truth.jsonl")
    with open(gt_path, "w") as f:
        for item in gt:
            f.write(json.dumps(item) + "\n")
    print(f"  Ground truth pairs: {len(gt)} -> {gt_path}")

    # Timing data
    timing = extract_timing_data(all_windows)
    timing_path = os.path.join(OUT_DIR, "timing_data.jsonl")
    with open(timing_path, "w") as f:
        for item in timing:
            f.write(json.dumps(item) + "\n")
    print(f"  Timing data points: {len(timing)} -> {timing_path}")

    # Stats
    seth_msgs = [m for m in messages if m["is_from_me"]]
    other_msgs = [m for m in messages if not m["is_from_me"]]
    seth_lengths = [len(m["text"]) for m in seth_msgs]
    print(f"\n--- Stats ---")
    print(f"  Seth messages: {len(seth_msgs)}")
    print(f"  Other messages: {len(other_msgs)}")
    if seth_lengths:
        print(f"  Seth avg length: {sum(seth_lengths)/len(seth_lengths):.0f} chars")
        print(f"  Seth median length: {sorted(seth_lengths)[len(seth_lengths)//2]} chars")
    if timing:
        delays = [t["delay_seconds"] for t in timing]
        print(f"  Avg reply delay: {sum(delays)/len(delays):.0f}s")
        print(f"  Median reply delay: {sorted(delays)[len(delays)//2]:.0f}s")

    # Sample for inspection
    print(f"\n--- Sample training pair ---")
    if training:
        print(json.dumps(training[0], indent=2))
    print(f"\n--- Sample ground truth ---")
    if gt:
        print(json.dumps(gt[0], indent=2))


if __name__ == "__main__":
    main()
