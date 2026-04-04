#!/usr/bin/env python3
"""
Prepare unified texting training data from multiple sources:
  1. Personal iMessage history (chat.db)
  2. REALTALK dataset (authentic 21-day messaging conversations)
  3. NUS SMS Corpus (67K real SMS messages)
  4. DailyDialog (casual multi-turn conversations)

Outputs conversation-formatted text ready for tokenization by `human ml prepare`.

Format per conversation turn:
  <|user|>message text<|end|>
  <|other|>message text<|end|>

Usage:
  python3 scripts/prepare-texting-data.py [--output-dir ~/.human/training-data/prepared]
"""

import argparse
import glob
import json
import os
import random
import sqlite3
import sys
import xml.etree.ElementTree as ET


def collect_imessage(chat_db_path):
    """Extract conversations from local iMessage chat.db, grouped by contact."""
    if not os.path.exists(chat_db_path):
        print(f"  [imessage] chat.db not found at {chat_db_path}, skipping")
        return []

    conversations = []
    try:
        conn = sqlite3.connect(f"file:{chat_db_path}?mode=ro", uri=True)
        cursor = conn.cursor()
        cursor.execute("""
            SELECT h.id, m.is_from_me, m.text,
                   m.date/1000000000 + 978307200 as ts
            FROM message m
            JOIN handle h ON m.handle_id = h.ROWID
            WHERE m.text IS NOT NULL AND m.text != ''
            ORDER BY h.id, m.date ASC
        """)

        current_contact = None
        current_convo = []
        for handle_id, is_from_me, text, ts in cursor.fetchall():
            if handle_id != current_contact:
                if current_convo and len(current_convo) >= 2:
                    conversations.append(current_convo)
                current_contact = handle_id
                current_convo = []
            role = "<|user|>" if is_from_me else "<|other|>"
            current_convo.append(f"{role}{text}<|end|>")

        if current_convo and len(current_convo) >= 2:
            conversations.append(current_convo)
        conn.close()
    except Exception as e:
        print(f"  [imessage] error reading chat.db: {e}")

    total = sum(len(c) for c in conversations)
    print(f"  [imessage] {total} messages in {len(conversations)} conversations")
    return conversations


def collect_realtalk(data_dir):
    """Extract conversations from REALTALK dataset JSON files."""
    conversations = []
    json_files = glob.glob(os.path.join(data_dir, "*.json"))
    json_files += glob.glob(os.path.join(data_dir, "locomo", "*.json"))

    for fpath in json_files:
        try:
            data = json.load(open(fpath))
            names = data.get("name", {})
            speaker1 = names.get("speaker_1", "A")
            speaker2 = names.get("speaker_2", "B")

            for key in sorted(data.keys()):
                if not key.startswith("session_") or key.endswith("date_time"):
                    continue
                if key.startswith("events_"):
                    continue
                session = data[key]
                if not isinstance(session, list):
                    continue

                convo = []
                for msg in session:
                    text = msg.get("clean_text", "").strip()
                    if not text:
                        continue
                    speaker = msg.get("speaker", "")
                    role = "<|user|>" if speaker == speaker1 else "<|other|>"
                    convo.append(f"{role}{text}<|end|>")

                if len(convo) >= 2:
                    conversations.append(convo)
        except Exception as e:
            print(f"  [realtalk] error in {fpath}: {e}")

    total = sum(len(c) for c in conversations)
    print(f"  [realtalk] {total} messages in {len(conversations)} conversation sessions")
    return conversations


def collect_nus_sms(xml_path):
    """Extract messages from NUS SMS Corpus XML.
    SMS are individual messages without conversation threading,
    so we group them into synthetic pairs for training."""
    if not os.path.exists(xml_path):
        print(f"  [nus-sms] XML not found at {xml_path}, skipping")
        return []

    messages = []
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()
        for msg_elem in root.findall(".//message"):
            text_elem = msg_elem.find("text")
            if text_elem is not None and text_elem.text:
                text = text_elem.text.strip()
                if len(text) > 3:
                    messages.append(text)
    except Exception as e:
        print(f"  [nus-sms] error parsing XML: {e}")
        return []

    random.shuffle(messages)
    conversations = []
    for i in range(0, len(messages) - 1, 2):
        convo = [
            f"<|other|>{messages[i]}<|end|>",
            f"<|user|>{messages[i+1]}<|end|>",
        ]
        conversations.append(convo)

    total = sum(len(c) for c in conversations)
    print(f"  [nus-sms] {total} messages in {len(conversations)} synthetic pairs")
    return conversations


def collect_dailydialog():
    """Load DailyDialog dataset from Hugging Face."""
    conversations = []
    try:
        from datasets import load_dataset
        ds = load_dataset("li2017dailydialog/daily_dialog", split="train",
                          trust_remote_code=True)
        for row in ds:
            dialog = row.get("dialog", [])
            if len(dialog) < 2:
                continue
            convo = []
            for i, utterance in enumerate(dialog):
                text = utterance.strip()
                if not text:
                    continue
                role = "<|user|>" if i % 2 == 0 else "<|other|>"
                convo.append(f"{role}{text}<|end|>")
            if len(convo) >= 2:
                conversations.append(convo)

        total = sum(len(c) for c in conversations)
        print(f"  [dailydialog] {total} messages in {len(conversations)} conversations")
    except ImportError:
        print("  [dailydialog] 'datasets' library not installed, skipping")
    except Exception as e:
        print(f"  [dailydialog] error: {e}")

    return conversations


def write_training_data(conversations, output_dir):
    """Write conversations to text files for tokenization."""
    os.makedirs(output_dir, exist_ok=True)

    random.shuffle(conversations)

    all_text = []
    for convo in conversations:
        all_text.append("<|convo|>\n")
        all_text.append("\n".join(convo))
        all_text.append("\n<|endconvo|>\n\n")

    full_text = "".join(all_text)

    split_point = int(len(conversations) * 0.9)
    train_convos = conversations[:split_point]
    val_convos = conversations[split_point:]

    train_text = []
    for convo in train_convos:
        train_text.append("<|convo|>\n")
        train_text.append("\n".join(convo))
        train_text.append("\n<|endconvo|>\n\n")

    val_text = []
    for convo in val_convos:
        val_text.append("<|convo|>\n")
        val_text.append("\n".join(convo))
        val_text.append("\n<|endconvo|>\n\n")

    train_path = os.path.join(output_dir, "train.txt")
    val_path = os.path.join(output_dir, "val.txt")
    all_path = os.path.join(output_dir, "all.txt")

    with open(train_path, "w") as f:
        f.write("".join(train_text))
    with open(val_path, "w") as f:
        f.write("".join(val_text))
    with open(all_path, "w") as f:
        f.write(full_text)

    train_msgs = sum(len(c) for c in train_convos)
    val_msgs = sum(len(c) for c in val_convos)
    print(f"\n  Written to {output_dir}:")
    print(f"    train.txt: {len(train_convos)} convos, {train_msgs} messages, "
          f"{len(''.join(train_text)):,} chars")
    print(f"    val.txt:   {len(val_convos)} convos, {val_msgs} messages, "
          f"{len(''.join(val_text)):,} chars")
    print(f"    all.txt:   {len(conversations)} convos total")


def main():
    parser = argparse.ArgumentParser(description="Prepare texting training data")
    parser.add_argument("--output-dir", default=os.path.expanduser(
        "~/.human/training-data/prepared"))
    parser.add_argument("--chat-db", default=os.path.expanduser(
        "~/Library/Messages/chat.db"))
    parser.add_argument("--realtalk-dir", default=os.path.expanduser(
        "~/.human/training-data/datasets/realtalk/data"))
    parser.add_argument("--nus-sms-xml", default=os.path.expanduser(
        "~/.human/training-data/datasets/nus-sms-english/smsCorpus_en_2015.03.09_all.xml"))
    parser.add_argument("--skip-dailydialog", action="store_true")
    args = parser.parse_args()

    print("Collecting training data from all sources...\n")
    all_conversations = []

    print("[1/4] Personal iMessage history")
    all_conversations.extend(collect_imessage(args.chat_db))

    print("[2/4] REALTALK dataset")
    all_conversations.extend(collect_realtalk(args.realtalk_dir))

    print("[3/4] NUS SMS Corpus")
    all_conversations.extend(collect_nus_sms(args.nus_sms_xml))

    if not args.skip_dailydialog:
        print("[4/4] DailyDialog (Hugging Face)")
        all_conversations.extend(collect_dailydialog())
    else:
        print("[4/4] DailyDialog — skipped")

    total_msgs = sum(len(c) for c in all_conversations)
    print(f"\n{'='*50}")
    print(f"Total: {total_msgs:,} messages in {len(all_conversations):,} conversations")
    print(f"{'='*50}")

    write_training_data(all_conversations, args.output_dir)
    print("\nDone! Run 'human ml prepare <output-dir>' to tokenize.")


if __name__ == "__main__":
    main()
