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
import re
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


def extract_text_from_attributed_body(blob):
    """Decode text from an NSAttributedString (NSKeyedArchiver) blob.

    Modern macOS stores iMessage text only in the attributedBody column as a
    serialised NSAttributedString.  The text payload sits between the
    ``NSString`` marker and a ``\\x86`` terminator.
    """
    idx = blob.find(b"NSString")
    if idx < 0:
        return None
    start = blob.find(b"+", idx)
    if start < 0:
        return None
    start += 1
    end = blob.find(b"\x86", start)
    if end < 0:
        end = start + 2000
    raw = blob[start:end]
    try:
        text = raw.decode("utf-8", errors="ignore").strip()
    except Exception:
        return None
    text = re.sub(r"^[\x00-\x1f]+", "", text)
    return text if len(text) > 1 else None


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
            m.date_read,
            m.attributedBody
        FROM message m
        LEFT JOIN handle h ON m.handle_id = h.ROWID
        LEFT JOIN chat_message_join cmj ON m.ROWID = cmj.message_id
        LEFT JOIN chat c ON cmj.chat_id = c.ROWID
        WHERE m.item_type = 0
          AND m.associated_message_type = 0
          AND (m.text IS NOT NULL OR m.attributedBody IS NOT NULL)
        ORDER BY c.chat_identifier, m.date
    """)

    messages = []
    ab_recovered = 0
    for row in cursor.fetchall():
        rowid, is_from_me, text, date_ns, contact, chat_id, delivered, read, attr_body = row

        if text and text.strip():
            final_text = text.strip()
        elif attr_body:
            final_text = extract_text_from_attributed_body(attr_body)
            if final_text:
                ab_recovered += 1
        else:
            continue

        if not final_text or should_skip(final_text):
            continue

        unix_ts = apple_date_to_unix(date_ns)
        messages.append({
            "rowid": rowid,
            "is_from_me": bool(is_from_me),
            "text": final_text,
            "timestamp": unix_ts,
            "contact": contact,
            "chat_id": chat_id,
            "datetime": datetime.fromtimestamp(unix_ts).isoformat() if unix_ts > 0 else "",
            "delivered_ts": apple_date_to_unix(delivered) if delivered else 0,
            "read_ts": apple_date_to_unix(read) if read else 0,
        })

    conn.close()
    if ab_recovered:
        print(f"  ({ab_recovered} messages recovered from attributedBody)")
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


def extract_voice_training_pairs(windows):
    """
    Extract training pairs optimized for real-time voice (E4B/E2B models).

    Voice-optimized differences from standard training:
      - Shorter context windows (3 messages max — voice is more immediate)
      - Prefer shorter Seth replies (voice needs concise responses)
      - Add conversational timing metadata for pacing
      - Filter to conversational-style messages (not links, not long-form)
    """
    MAX_VOICE_REPLY_LENGTH = 280
    MAX_VOICE_CONTEXT = 3
    pairs = []
    for window in windows:
        for i, msg in enumerate(window):
            if not msg["is_from_me"] or len(msg["text"]) < MIN_REPLY_LENGTH:
                continue
            if len(msg["text"]) > MAX_VOICE_REPLY_LENGTH:
                continue
            if msg["text"].startswith("http") or "\n\n" in msg["text"]:
                continue

            context_start = max(0, i - MAX_VOICE_CONTEXT)
            context = window[context_start:i]
            if not context:
                continue

            messages = []
            for ctx in context:
                role = "assistant" if ctx["is_from_me"] else "user"
                messages.append({"role": role, "content": ctx["text"]})
            messages.append({"role": "assistant", "content": msg["text"]})

            delay_from_prev = 0
            if i > 0:
                delay_from_prev = msg["timestamp"] - window[i - 1]["timestamp"]

            pairs.append({
                "messages": messages,
                "metadata": {
                    "chat_id": msg["chat_id"],
                    "timestamp": msg["datetime"],
                    "reply_length": len(msg["text"]),
                    "response_delay_s": round(delay_from_prev, 1),
                    "format": "voice",
                }
            })
    return pairs


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

    # Training pairs (standard — for 31B fine-tuning)
    training = extract_training_pairs(all_windows)
    train_path = os.path.join(OUT_DIR, "training_pairs.jsonl")
    with open(train_path, "w") as f:
        for pair in training:
            f.write(json.dumps(pair) + "\n")
    print(f"\n  Training pairs: {len(training)} -> {train_path}")

    # Voice-optimized training pairs (for E4B/E2B fine-tuning)
    voice_training = extract_voice_training_pairs(all_windows)
    voice_path = os.path.join(OUT_DIR, "voice_training_pairs.jsonl")
    with open(voice_path, "w") as f:
        for pair in voice_training:
            f.write(json.dumps(pair) + "\n")
    print(f"  Voice training pairs: {len(voice_training)} -> {voice_path}")

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

    # Voice-specific stats
    voice_lengths = [len(p["messages"][-1]["content"]) for p in voice_training]
    if voice_lengths:
        print(f"\n--- Voice Training Stats ---")
        print(f"  Voice pairs: {len(voice_training)} ({len(voice_training)/len(training)*100:.0f}% of total)")
        print(f"  Avg voice reply length: {sum(voice_lengths)/len(voice_lengths):.0f} chars")
        print(f"  Median voice reply length: {sorted(voice_lengths)[len(voice_lengths)//2]} chars")

    # Sample for inspection
    print(f"\n--- Sample training pair ---")
    if training:
        print(json.dumps(training[0], indent=2))
    print(f"\n--- Sample voice training pair ---")
    if voice_training:
        print(json.dumps(voice_training[0], indent=2))
    print(f"\n--- Sample ground truth ---")
    if gt:
        print(json.dumps(gt[0], indent=2))


if __name__ == "__main__":
    main()
