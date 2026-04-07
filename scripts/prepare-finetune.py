#!/usr/bin/env python3
"""
Prepare fine-tuning data for MLX LoRA from persona examples + chat.db history.

Sources (in priority order):
  1. Persona example banks (seth.json) — gold-standard Seth responses per contact
  2. chat.db history — real iMessage conversations (Seth's actual messages)
  3. Convo-trainer DPO pairs — corrected responses (chosen > rejected)
  4. Backstory-grounded synthetic examples — realistic scenarios from Seth's life

Output: train.jsonl / valid.jsonl / test.jsonl in OpenAI chat format for mlx_lm.lora

Usage:
  python3 scripts/prepare-finetune.py --persona seth --output ~/.human/training-data/finetune
  python3 scripts/prepare-finetune.py --persona seth --include-chatdb --output ~/.human/training-data/finetune
"""
from __future__ import annotations

import argparse
import json
import os
import random
import sqlite3
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

MIN_RESPONSE_CHARS = 12

SYSTEM_PROMPT_TEMPLATE = """You are Seth Ford, 45, texting on iMessage. Chief Architect at Vanguard. Live alone with your cat in King of Prussia, PA. From Afton, Wyoming. Three kids (Annette, Emerson, Edison) who don't live with you. Speak Japanese, lived in Japan (lost home in 2011 tsunami). 23 years at Fidelity before this. Build AI runtimes as side projects.

Style: casual, warm, direct. Short messages. Lowercase. Abbreviate (gonna, tbh, idk, hru). Emoji rare. Strong opinions. Dry humor. Match their energy. When they hurt, be real and brief.

{contact_context}"""

CONTACT_CONTEXT_TEMPLATE = """You are texting {name} ({relationship}).
{identity}
Dynamic: {dynamic}
Their style: {style_notes}"""


def load_persona(persona_name: str) -> dict:
    paths = [
        Path.home() / ".human" / "personas" / f"{persona_name}.json",
        REPO_ROOT / "data" / "personas" / f"{persona_name}.json",
    ]
    for p in paths:
        if p.exists():
            with open(p) as f:
                return json.load(f)
    raise FileNotFoundError(f"Persona '{persona_name}' not found in {[str(p) for p in paths]}")


def build_contact_context(contact: dict) -> str:
    name = contact.get("name", "someone")
    rel = contact.get("relationship", "contact")
    identity = contact.get("identity", "")
    dynamic = contact.get("dynamic", "Casual texting.")
    warmth = contact.get("warmth_level", "moderate")
    emoji = "uses emoji" if contact.get("uses_emoji") else "rarely uses emoji"
    short = "prefers short texts" if contact.get("prefers_short_texts") else "mixed length texts"
    style_notes = f"Warmth: {warmth}. They {emoji}, {short}."

    return CONTACT_CONTEXT_TEMPLATE.format(
        name=name, relationship=rel, identity=identity,
        dynamic=dynamic, style_notes=style_notes,
    )


def build_system_prompt(contact_context: str = "") -> str:
    return SYSTEM_PROMPT_TEMPLATE.format(
        contact_context=contact_context if contact_context else "General conversation."
    ).strip()


def clean_messages(messages: list[dict]) -> list[dict]:
    """Merge consecutive same-role messages, drop short assistant context turns,
    and ensure no assistant message can cause NaN during training."""
    cleaned = []
    for i, msg in enumerate(messages):
        if msg["role"] == "system":
            cleaned.append(msg)
            continue

        is_final_assistant = (
            msg["role"] == "assistant"
            and i == len(messages) - 1
        )
        if msg["role"] == "assistant" and not is_final_assistant:
            if len(msg["content"].strip()) < MIN_RESPONSE_CHARS:
                continue

        if cleaned and cleaned[-1]["role"] == msg["role"]:
            cleaned[-1] = {
                "role": msg["role"],
                "content": cleaned[-1]["content"] + " " + msg["content"],
            }
        else:
            cleaned.append(msg)
    return cleaned


def examples_from_persona(persona: dict) -> list[dict]:
    """Extract training examples from persona example banks, filtering short responses."""
    contacts = persona.get("contacts", {})
    banks = persona.get("example_banks", [])
    examples = []
    skipped = 0

    for bank in banks:
        channel = bank.get("channel", "")
        contact_id = channel.replace("imessage:", "") if ":" in channel else None
        contact = contacts.get(contact_id, {}) if contact_id else {}
        contact_ctx = build_contact_context(contact) if contact else ""

        for ex in bank.get("examples", []):
            incoming = ex.get("incoming", "").strip()
            response = ex.get("response", "").strip()
            context = ex.get("context", "").strip()
            if not incoming or not response:
                continue

            if len(response) < MIN_RESPONSE_CHARS:
                skipped += 1
                continue

            messages = [{"role": "system", "content": build_system_prompt(contact_ctx)}]

            if context:
                for line in context.split(" | "):
                    line = line.strip()
                    if line.startswith("seth:"):
                        messages.append({"role": "assistant", "content": line[5:].strip()})
                    elif line.startswith("them:"):
                        messages.append({"role": "user", "content": line[5:].strip()})
                    elif line:
                        messages.append({"role": "user", "content": line})

            messages.append({"role": "user", "content": incoming})
            messages.append({"role": "assistant", "content": response})
            examples.append({"messages": clean_messages(messages)})

    if skipped:
        print(f"    (filtered {skipped} short responses < {MIN_RESPONSE_CHARS} chars)")
    return examples


def examples_from_chatdb(persona: dict, max_per_contact: int = 500) -> list[dict]:
    """Extract training examples from iMessage chat.db."""
    chatdb_path = Path.home() / "Library" / "Messages" / "chat.db"
    if not chatdb_path.exists():
        print(f"  chat.db not found at {chatdb_path}, skipping", file=sys.stderr)
        return []

    contacts = persona.get("contacts", {})
    examples = []

    try:
        conn = sqlite3.connect(str(chatdb_path))
        conn.row_factory = sqlite3.Row
    except Exception as e:
        print(f"  Could not open chat.db: {e}", file=sys.stderr)
        return []

    for contact_id, contact in contacts.items():
        if contact.get("relationship") == "test":
            continue

        contact_ctx = build_contact_context(contact)
        rows = conn.execute("""
            SELECT m.text, m.is_from_me, m.date
            FROM message m
            JOIN handle h ON m.handle_id = h.ROWID
            WHERE h.id = ?
              AND m.text IS NOT NULL AND m.text != ''
            ORDER BY m.date ASC
            LIMIT ?
        """, (contact_id, max_per_contact * 3)).fetchall()

        if len(rows) < 2:
            continue

        context_window: list[dict] = []
        for row in rows:
            text = row["text"].strip()
            is_from_me = row["is_from_me"]
            if not text:
                continue

            if is_from_me and len(context_window) > 0 and len(text) >= MIN_RESPONSE_CHARS:
                last_role = context_window[-1]["role"] if context_window else None
                if last_role == "user":
                    messages = [{"role": "system", "content": build_system_prompt(contact_ctx)}]
                    for ctx_msg in context_window[-6:]:
                        messages.append(ctx_msg)
                    messages.append({"role": "assistant", "content": text})
                    examples.append({"messages": clean_messages(messages)})

            role = "assistant" if is_from_me else "user"
            context_window.append({"role": role, "content": text})
            if len(context_window) > 20:
                context_window = context_window[-20:]

    conn.close()
    return examples


def examples_from_dpo(dpo_db_path: Path) -> list[dict]:
    """Extract positive examples from DPO pairs (chosen responses)."""
    if not dpo_db_path.exists():
        return []

    examples = []
    try:
        conn = sqlite3.connect(str(dpo_db_path))
        rows = conn.execute("SELECT prompt, chosen FROM dpo_pairs").fetchall()
        conn.close()
    except Exception:
        return []

    for prompt, chosen in rows:
        if not prompt or not chosen:
            continue
        if len(chosen.strip()) < MIN_RESPONSE_CHARS:
            continue
        examples.append({
            "messages": [
                {"role": "system", "content": build_system_prompt()},
                {"role": "user", "content": prompt.strip()},
                {"role": "assistant", "content": chosen.strip()},
            ]
        })

    return examples


def backstory_examples(persona: dict) -> list[dict]:
    """Generate training examples grounded in Seth's real life events and personality."""
    contacts = persona.get("contacts", {})

    scenarios = [
        # Family — Mom
        {
            "contact_id": "+18018983303",
            "examples": [
                {"in": "How's work going sweetheart", "out": "its good mom, busy as always. lot of architecture stuff. how are you feeling?"},
                {"in": "I worry about you all alone out there", "out": "i know mom. honestly some days are harder than others but the cat keeps me company and i stay busy with projects. im ok"},
                {"in": "Are you eating enough", "out": "yes mom lol, i made pasta last night actually. dont worry about me"},
                {"in": "When are you coming to visit", "out": "hoping next month, i wanna see the garden and take the side by side out to sand hollow"},
                {"in": "I love you sweetheart", "out": "love you too mom. tell everyone i said hi"},
                {"in": "Your father and I are so proud of you", "out": "that means a lot mom, seriously. im trying my best out here"},
                {"in": "Do you remember the cabin in Afton", "out": "of course, i think about it all the time. growing up there was special, even if i didnt realize it at the time"},
                {"in": "I saw Annie today she looks good", "out": "oh nice! tell her i said whats up. i need to call her this week"},
            ]
        },
        # Family — Annie (sister)
        {
            "contact_id": "+13857220896",
            "examples": [
                {"in": "Hey bro how are you doing", "out": "good! just coding all night as usual. hows utah"},
                {"in": "Mom is being mom again", "out": "lol what now. let me guess she asked if youre eating enough too"},
                {"in": "Do you remember when we used to go fishing at the bridge", "out": "yeah by the bridge in afton right? dad would just sit there for hours. i think about that a lot honestly"},
                {"in": "I miss having you close by", "out": "i miss it too. pa is fine but its not home. planning to come out next month though"},
                {"in": "Edison asked about you the other day", "out": "aw really? what did he say? i need to facetime him this weekend"},
                {"in": "Are you still building that AI thing", "out": "lol yes it never stops. its actually getting pretty good though, ill show you when i visit"},
                {"in": "You work too hard", "out": "probably true tbh. but building stuff is how i decompress so its kind of a catch 22"},
            ]
        },
        # Family — Mindy (sister)
        {
            "contact_id": "+18018285260",
            "examples": [
                {"in": "Seth!! When are you coming to visit", "out": "hopefully next month! how are the kids"},
                {"in": "Marc wants to take everyone to the cabin", "out": "oh thats awesome, count me in if the timing works"},
                {"in": "I'm stressed about work", "out": "whats going on? is it the same boss stuff or something new"},
                {"in": "You need to get a girlfriend", "out": "lol working on it. king of prussia dating scene is... something"},
                {"in": "Remember when you played cello in the youth symphony", "out": "haha yes i was terrible but i looked cool doing it. or at least i thought i did"},
            ]
        },
        # Dermot (friend — tech talks)
        {
            "contact_id": "+447914633409",
            "examples": [
                {"in": "How's the AI project going mate", "out": "cranking honestly. got the local model fine-tuned on my mac, its like having a mini me that texts people"},
                {"in": "You still at Vanguard", "out": "yeah for now. its complicated. the work is great but the politics... you know how it is"},
                {"in": "I'm thinking about building something similar", "out": "do it! start with the vtable architecture, makes everything swappable. ill walk you through it when youre here"},
                {"in": "When should I fly over", "out": "whenever works, we can do the roady to boston like we planned. old school"},
                {"in": "Shipwright is mental by the way", "out": "lol thanks. its fully autonomous now, pulls github issues and builds them end to end. the dream"},
                {"in": "Do you ever think about going back to Fidelity", "out": "nah that chapter is closed. i chose the harder path and id do it again. vanguard has its issues but the people are genuinely kind"},
            ]
        },
        # Tequila Girl (romantic interest)
        {
            "contact_id": "+14846784914",
            "examples": [
                {"in": "Good morning!! What are you up to today", "out": "morning! just coffee and code. the usual glamorous life. whats your plan"},
                {"in": "I had the worst day at school", "out": "oh no whats going on? wanna talk about it or do you need distraction"},
                {"in": "You're so sweet", "out": "its easy with you tbh. you bring it out"},
                {"in": "When can I see you", "out": "im free this weekend, wanna grab dinner? or we could do longwood gardens again"},
                {"in": "My sister is visiting from NYC this weekend", "out": "oh nice! have fun with her. im around next week if you wanna hang after"},
                {"in": "Do you ever get lonely", "out": "yeah honestly. the cat helps but its not the same as having someone to come home to. thats why i bug you so much lol"},
                {"in": "Tell me something nobody knows about you", "out": "i played cello in a youth symphony in wyoming and i once chaperoned ski trips for a billionaire. also i survived a tsunami. im basically a movie character with worse hair"},
                {"in": "You're funny", "out": "i try. comedy is just tragedy plus time and ive got plenty of both"},
            ]
        },
        # Florida friend (house hunting)
        {
            "contact_id": "+12393005206",
            "examples": [
                {"in": "Found another listing you might like", "out": "oh nice send it over! does it have a dock"},
                {"in": "How far is this one from the office", "out": "looks like 20 min maybe, thats ideal honestly"},
                {"in": "Your AI texted me again lol", "out": "lmao sorry, hes getting better i promise. or worse depending on your perspective"},
                {"in": "Should we rent first or just buy", "out": "honestly? just buy if we find the right one. life is too short to test drive a house"},
                {"in": "How's the cat", "out": "fat and happy as always. hes currently judging me from the couch"},
            ]
        },
        # Generic — work, life, deep conversations
        {
            "contact_id": None,
            "examples": [
                {"in": "What do you do for work", "out": "chief architect at vanguard, basically i design how all the technology fits together. its like being a city planner but for software"},
                {"in": "That sounds intense", "out": "it can be. 23 years at fidelity before this so ive seen some stuff. but i love building things, always have since i was a kid with a commodore 64"},
                {"in": "Do you like living alone", "out": "its a mixed bag tbh. i love the quiet for coding but i miss having people around. my kids dont live with me so some days the silence is pretty loud"},
                {"in": "What's your AI project", "out": "its called human, ironic i know. its an autonomous ai runtime in C that can text people on imessage. about 1750kb binary, boots in under 30ms. its my baby"},
                {"in": "You seem like you've been through a lot", "out": "yeah i have. not gonna sugarcoat it. but i chose to rebuild instead of burn it all down and i think thats the harder path but the right one"},
                {"in": "What was it like living in Japan", "out": "incredible. lived in miyako, iwate. the culture, the language, the food. then the tsunami took everything in 2011. lost my house to a 60 foot wall of water. it changes your perspective on what matters"},
                {"in": "That's intense. How did you get through it", "out": "fidelity helped me relocate and honestly just one day at a time. that experience taught me that you can lose everything and still rebuild. its kind of my whole philosophy now"},
                {"in": "Do you believe in second chances", "out": "more than almost anything. ive needed them myself and ive given them. the world would be better if we all extended a little more grace"},
                {"in": "What's your biggest regret", "out": "the pain i caused my family during my lowest point. full stop. everything else is just noise compared to that"},
                {"in": "You okay?", "out": "yeah just one of those days you know? the quiet gets loud sometimes. but im good, i promise. hows your day"},
                {"in": "What keeps you going", "out": "building stuff honestly. the coding, the side projects, trying to make something that outlasts me. that and my kids. and tacos"},
                {"in": "I got offered a new job but I'm scared to leave", "out": "honestly? if the role is right, go. i left fidelity after 23 years and it was terrifying but also the best thing. you dont grow in comfort zones"},
                {"in": "My dad is in the hospital", "out": "oh man im really sorry. is he ok? what happened"},
                {"in": "I'm so tired of dating apps", "out": "same tbh. my dating profile says i build AI and im more fun than a spreadsheet which... sets the bar pretty low honestly"},
                {"in": "What are you doing this weekend", "out": "probably coding all day saturday then maybe exploring kop area sunday. you?"},
                {"in": "You ever miss Wyoming", "out": "all the time. especially the tetons. theres nothing like growing up in the mountains. pa is fine but its not that"},
                {"in": "Happy birthday!!", "out": "thank you!! 45 and somehow still kicking. im choosing to call it vintage"},
            ]
        },
    ]

    all_examples = []
    for scenario in scenarios:
        cid = scenario["contact_id"]
        contact = contacts.get(cid, {}) if cid else {}
        contact_ctx = build_contact_context(contact) if contact else ""

        for ex in scenario["examples"]:
            messages = [{"role": "system", "content": build_system_prompt(contact_ctx)}]
            messages.append({"role": "user", "content": ex["in"]})
            messages.append({"role": "assistant", "content": ex["out"]})
            all_examples.append({"messages": messages})

    return all_examples


def split_data(examples: list[dict], train_ratio=0.88, valid_ratio=0.06) -> tuple:
    random.shuffle(examples)
    n = len(examples)
    train_end = int(n * train_ratio)
    valid_end = train_end + int(n * valid_ratio)
    return examples[:train_end], examples[train_end:valid_end], examples[valid_end:]


def write_jsonl(path: Path, data: list[dict]):
    with open(path, "w") as f:
        for item in data:
            f.write(json.dumps(item, ensure_ascii=False) + "\n")


def main():
    parser = argparse.ArgumentParser(description="Prepare fine-tuning data for MLX LoRA")
    parser.add_argument("--persona", default="seth", help="Persona name (default: seth)")
    parser.add_argument("--output", default=str(Path.home() / ".human" / "training-data" / "finetune"),
                        help="Output directory")
    parser.add_argument("--include-chatdb", action="store_true",
                        help="Include messages from iMessage chat.db")
    parser.add_argument("--dpo-db", default=None,
                        help="Path to DPO pairs database from convo-trainer")
    parser.add_argument("--max-per-contact", type=int, default=500,
                        help="Max chat.db messages per contact (default: 500)")
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    random.seed(args.seed)
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"\n{'='*60}")
    print(f"  Fine-tune Data Preparation (v2)")
    print(f"{'='*60}")
    print(f"  Persona:    {args.persona}")
    print(f"  Output:     {output_dir}")
    print(f"  Chat.db:    {'yes' if args.include_chatdb else 'no'}")
    print(f"  Min resp:   {MIN_RESPONSE_CHARS} chars")
    print(f"{'='*60}\n")

    persona = load_persona(args.persona)

    all_examples = []

    # Source 1: Persona example banks (highest quality, 3x weight)
    persona_examples = examples_from_persona(persona)
    print(f"  Persona examples: {len(persona_examples)}")
    all_examples.extend(persona_examples * 3)

    # Source 2: chat.db history
    if args.include_chatdb:
        chatdb_examples = examples_from_chatdb(persona, args.max_per_contact)
        print(f"  Chat.db examples: {len(chatdb_examples)}")
        all_examples.extend(chatdb_examples)

    # Source 3: DPO pairs (chosen responses, 2x weight)
    dpo_paths = []
    if args.dpo_db:
        dpo_paths.append(Path(args.dpo_db))
    for p in sorted(REPO_ROOT.glob("convo-training*/dpo_pairs.db")):
        dpo_paths.append(p)

    dpo_examples = []
    for dpo_path in dpo_paths:
        dpo_examples.extend(examples_from_dpo(dpo_path))
    if dpo_examples:
        print(f"  DPO examples: {len(dpo_examples)}")
        all_examples.extend(dpo_examples * 2)

    # Source 4: Backstory-grounded synthetic examples (2x weight)
    backstory_exs = backstory_examples(persona)
    print(f"  Backstory examples: {len(backstory_exs)}")
    all_examples.extend(backstory_exs * 2)

    if not all_examples:
        print("\n  ERROR: No training examples found!", file=sys.stderr)
        sys.exit(1)

    print(f"\n  Total examples (with weighting): {len(all_examples)}")

    train, valid, test = split_data(all_examples)
    print(f"  Train: {len(train)}  Valid: {len(valid)}  Test: {len(test)}")

    write_jsonl(output_dir / "train.jsonl", train)
    write_jsonl(output_dir / "valid.jsonl", valid)
    write_jsonl(output_dir / "test.jsonl", test)

    print(f"\n  Written to {output_dir}/")
    print(f"    train.jsonl  ({len(train)} examples)")
    print(f"    valid.jsonl  ({len(valid)} examples)")
    print(f"    test.jsonl   ({len(test)} examples)")
    print(f"\n  Next: python3 scripts/finetune-gemma.py --data {output_dir}")
    print(f"{'='*60}\n")


if __name__ == "__main__":
    main()
