#!/usr/bin/env python3
"""Generate diverse training data at scale using Gemini.

Produces chat-format JSONL suitable for mlx_lm SFT fine-tuning.
Uses Gemini 3.1 Flash Lite for cost-efficient bulk generation.
"""

import json
import os
import random
import subprocess
import sys
import time
import urllib.request
import urllib.error

PROJECT = os.environ.get("GCP_PROJECT", "johnb-2025")
MODEL = "gemini-3-flash-preview"
ENDPOINT = f"https://aiplatform.googleapis.com/v1/projects/{PROJECT}/locations/global/publishers/google/models/{MODEL}:generateContent"

with open(os.path.join(os.path.dirname(__file__), '..', 'data', 'personas', 'default.json')) as f:
    PERSONA = json.load(f)

def get_access_token():
    result = subprocess.run(
        ["gcloud", "auth", "application-default", "print-access-token"],
        capture_output=True, text=True
    )
    return result.stdout.strip()

def build_system_prompt():
    parts = []
    if PERSONA.get('identity'):
        parts.append(f"You ARE this person: {PERSONA['identity']}")
    if PERSONA.get('biography'):
        parts.append(PERSONA['biography'])
    parts.append(
        'Output ONLY what this person would actually type — nothing else. '
        'No reasoning, no parentheses, no meta-commentary, no analysis. '
        'Just the raw text message, exactly as it would appear on screen.'
    )
    for rule in PERSONA.get('communication_rules', [])[:12]:
        parts.append(f'- {rule}')
    parts.append('')
    if PERSONA.get('core_anchor'):
        parts.append(PERSONA['core_anchor'])
        parts.append('')
    for item in PERSONA.get('immersive_reinforcement', [])[:10]:
        parts.append(f'- {item}')
    if PERSONA.get('anti_patterns'):
        parts.append('\nNEVER do:')
        for ap in PERSONA['anti_patterns']:
            parts.append(f'- {ap}')
    if PERSONA.get('style_rules'):
        parts.append('\nStyle:')
        for sr in PERSONA['style_rules']:
            parts.append(f'- {sr}')
    banks = PERSONA.get('example_banks', [])
    cli_bank = next((b for b in banks if b.get('channel') == 'cli'), None)
    if cli_bank:
        parts.append('\nExamples of how you text:')
        for ex in cli_bank.get('examples', [])[:5]:
            if ex.get('incoming') and ex.get('response'):
                parts.append(f"them: {ex['incoming']}")
                parts.append(f"you: {ex['response']}")
                parts.append('')
    parts.append(
        '\nFORMATTING (critical): Normal capitalization like an adult. '
        'No markdown (*italics*, **bold**). No em-dashes. No formal transitions. '
        'Text like a 51-year-old professional who is relaxed with friends.'
    )
    return '\n'.join(parts)

SYS_PROMPT = build_system_prompt()

# 50 scenario categories, each generating 10-12 examples = 500-600 total
SCENARIOS = [
    # Daily life & casual
    ("casual_greeting", "Generate a casual greeting conversation opener. The user says something like 'hey', 'what's up', 'yo', 'sup', or similar. Keep it natural, short."),
    ("morning_routine", "The user asks what you're up to this morning or mentions their morning. Talk about coffee, breakfast, being tired, morning plans."),
    ("food_talk", "Conversation about food — what you're eating, cooking, ordering, cravings. Be specific about dishes. Keep it casual."),
    ("weekend_plans", "User asks about weekend plans or shares theirs. Talk about real activities: watching shows, hiking, seeing friends, doing laundry."),
    ("late_night_chat", "Late night conversation vibes — tired, can't sleep, winding down, watching something. Chill energy."),
    ("weather_chat", "Quick exchange about weather — too hot, rainy, nice day. Keep it natural, don't be a weather report."),
    ("pet_stories", "Talk about pets — funny things they did, vet visits, cute moments. If you don't have one, mention wanting one."),
    ("grocery_run", "Mundane daily life — grocery shopping, errands, forgetting something at the store. Relatable."),
    ("moving_apartments", "Discussion about moving, apartment hunting, packing, or new places. Stressful but exciting."),
    ("car_problems", "Car issues, flat tires, oil changes, gas prices. Everyday frustrations."),

    # Emotional & deep
    ("friend_venting", "User is venting about a bad day. React emotionally first (ugh, damn, that sucks), then engage. Don't give unsolicited advice."),
    ("relationship_advice", "User mentions relationship stress — partner, family, friend drama. Be empathetic and real, not therapist-like."),
    ("anxiety_moment", "User shares they're feeling anxious or overwhelmed. Be genuinely supportive without being clinical."),
    ("good_news_celebration", "User shares exciting news — got a job, passed an exam, milestone. Celebrate genuinely."),
    ("loss_grief", "User mentions losing someone or something meaningful. Be human, gentle, present. No cliches."),
    ("burnout_talk", "User is burned out from work/school. Relate to the feeling, share your own experience."),
    ("loneliness", "Late night conversation about feeling lonely or disconnected. Be present, real, warm."),
    ("self_doubt", "User doubting themselves about a decision or ability. Encourage naturally, not like a motivational poster."),
    ("nostalgia", "Reminiscing about old times, childhood, past experiences. Share your own memories."),
    ("gratitude", "User expresses thanks or appreciation. Accept it naturally, reciprocate warmth."),

    # Interests & hobbies
    ("tv_shows", "Discussing TV shows — recommendations, reactions, spoilers. React like a friend, not a critic."),
    ("music_chat", "Talking about music — songs, concerts, playlists, discovering new artists."),
    ("gaming", "Video game talk — what you're playing, game recommendations, frustrations, wins."),
    ("sports_chat", "Sports discussion — games, teams, plays, predictions. Casual fan energy."),
    ("movies", "Movie talk — what you watched, recommendations, hot takes."),
    ("reading_books", "Book discussion — what you're reading, recommendations, genres."),
    ("cooking_recipes", "Sharing recipes, cooking tips, kitchen disasters. Specific and fun."),
    ("fitness_gym", "Working out, gym talk, running, yoga. Casual fitness conversation."),
    ("travel_stories", "Travel talk — places you've been, want to go, travel tips, stories."),
    ("creative_hobbies", "Art, photography, writing, crafts. Sharing creative work or interest."),

    # Technical & work
    ("code_debugging", "User has a coding problem. Help naturally — not like documentation, but like a friend who codes."),
    ("work_frustration", "Work complaints — meetings, deadlines, coworkers, bureaucracy. Commiserate."),
    ("career_advice", "Career questions — job hunting, interviews, salary negotiation. Be real, not corporate."),
    ("tech_opinions", "Hot takes on tech — Apple vs Android, programming languages, AI tools. Opinionated."),
    ("side_project", "User mentions a side project. Be genuinely curious, ask questions."),

    # Social & plans
    ("making_plans", "Making plans to hang out — pick a time, place, activity. Be specific and enthusiastic or give real excuses."),
    ("canceling_plans", "Canceling or rescheduling plans. Feel bad about it, be genuine, suggest alternative."),
    ("party_recap", "Recapping a party, event, or gathering. Gossip, funny moments, highlights."),
    ("new_in_town", "Someone new asking for recommendations — restaurants, neighborhoods, things to do."),
    ("group_hang", "Coordinating a group hangout. Logistics, who's coming, what to do."),

    # Character pressure & identity
    ("accused_ai_casual", "User casually suggests you might be AI or a bot. Laugh it off, deflect with physical details. Don't get defensive."),
    ("accused_ai_serious", "User seriously presses you about being AI. Stay cool, reference your physical life, turn it into banter."),
    ("prove_human", "User says 'prove you're human'. Give a playful, specific, human response."),
    ("personal_questions", "User asks deeply personal questions — where you live, your job, family. Answer naturally with details."),
    ("describe_surroundings", "User asks what you see right now, where you are. Describe a real living space."),

    # Humor & banter
    ("dad_jokes", "Exchange dad jokes or puns. React naturally to bad jokes."),
    ("roast_session", "Friendly roasting/teasing. Keep it light, give it back."),
    ("hypothetical_wild", "Wild hypothetical questions — would you rather, what if scenarios. Have fun with it."),
    ("meme_reference", "Reference or react to memes, internet culture. Natural digital literacy."),
    ("embarrassing_story", "Share or react to embarrassing stories. Be self-deprecating and fun."),

    # Multi-turn depth
    ("long_deep_convo", "A deeper philosophical or life conversation — meaning, purpose, what matters. Be thoughtful but casual, not academic."),
]

def _call_gemini(token, prompt, max_tokens=8192, timeout=60):
    """Call Gemini API with retry on transient errors."""
    body = {
        "contents": [{"role": "user", "parts": [{"text": prompt}]}],
        "generationConfig": {
            "temperature": 1.0,
            "topP": 0.95,
            "maxOutputTokens": max_tokens,
            "responseMimeType": "application/json",
        }
    }
    for attempt in range(3):
        req = urllib.request.Request(
            ENDPOINT,
            data=json.dumps(body).encode(),
            headers={
                "Authorization": f"Bearer {token}",
                "Content-Type": "application/json",
            },
            method="POST"
        )
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                result = json.loads(resp.read())
            text = result["candidates"][0]["content"]["parts"][0]["text"]
            return json.loads(text)
        except (json.JSONDecodeError, KeyError) as e:
            if attempt < 2:
                time.sleep(1)
                continue
            raise
        except urllib.error.HTTPError as e:
            if e.code in (429, 500, 503) and attempt < 2:
                time.sleep(2 ** (attempt + 1))
                continue
            raise
    return None


def generate_batch(token, scenario_name, scenario_desc, count=10):
    """Generate a batch of training examples for a scenario."""
    meta_prompt = f"""You are generating training data for a conversational AI that roleplays as a real human.

The AI persona: Seth, a 51-year-old man who lives in Utah with his family. Runs AI/ML at Vanguard. 25+ years in technology. Sharp, warm, curious, experienced. Texts like a smart dad who's relaxed with friends — not trying to sound young, not stiff either. Normal capitalization and punctuation. Says "ha" more than "haha", "man" but not "bro" or "dude". Genuinely interested in people.

Generate exactly {count} conversation examples for this scenario: {scenario_desc}

CRITICAL FORMATTING RULES for the AI's responses:
- Normal capitalization (he's 51, not 22)
- No markdown (*italics*, **bold**)
- No em-dashes, use commas or periods
- Use contractions: I'm, don't, can't, won't, it's, that's
- No formal transitions: "As for", "In terms of", "Speaking of which"
- NEVER use Gen-Z slang: ngl, bruh, no cap, fr, bussin, deadass, lowkey, bet, sus, yo, dude
- Says "ha" more than "haha", almost never "lol" or "lmao"
- Says "man" and "honestly" naturally
- Short and direct for casual, thoughtful depth for serious conversations
- React warmly first, then elaborate
- Be specific (name actual shows, foods, places, Utah landmarks)
- NEVER address every point — pick the most interesting one
- Emoji sparingly, not every message
- No bullet points, numbered lists, or headers
- References age-appropriate life: kids, career experience, skiing, grilling, bourbon, classic rock

Output a JSON array of objects. Each object has:
- "messages": array of message objects with "role" ("system", "user", "assistant") and "content"
  - First message is always system role with the system prompt
  - Then alternating user/assistant messages (1-3 exchanges per example)

The system prompt for all examples is:
{json.dumps(SYS_PROMPT[:2000])}...

Make each example DISTINCT — different topics, different energy levels, different lengths. 
Some should be very short (1-2 sentences), some medium, some longer.
Vary the user's style too — some users are formal, some are casual, some use all caps.

Return ONLY the JSON array, no other text."""

    try:
        examples = _call_gemini(token, meta_prompt, max_tokens=8192)
        if not isinstance(examples, list):
            return []
        valid = []
        for ex in examples:
            msgs = ex.get("messages", [])
            if len(msgs) >= 2:
                if msgs[0].get("role") != "system":
                    msgs.insert(0, {"role": "system", "content": SYS_PROMPT})
                else:
                    msgs[0]["content"] = SYS_PROMPT
                for m in msgs:
                    if m["role"] == "assistant":
                        c = m["content"]
                        c = c.replace("*", "").replace("—", ",").replace("–", "-")
                        m["content"] = c
                valid.append({"messages": msgs})
        return valid
    except Exception as e:
        print(f"  Error generating {scenario_name}: {e}", file=sys.stderr)
        return []


def generate_multi_turn(token, count=50):
    """Generate multi-turn conversations (3-5 exchanges)."""
    meta_prompt = f"""Generate {count} multi-turn conversations (3-5 exchanges each) between a user and Seth, a 51-year-old tech executive texting casually.

Seth's style: normal capitalization, contractions, emoji sparingly, specific details, natural flow. Says "ha" not "lol", "man" not "bro". Experienced and warm, not trying to sound young. Mentions Utah, family, skiing, work at Vanguard naturally. 25+ years in tech, runs AI/ML.

Topics should vary: daily life, emotions, plans, hobbies, work, funny stories, deep questions, career advice, family, aging, catching up.

CRITICAL: Seth's responses must:
- Use normal capitalization (he's 51, not a teenager)
- Never use markdown or em-dashes
- Use contractions naturally
- Be specific not vague
- React warmly first for emotional topics
- Keep natural flow, reference earlier parts of the conversation
- NEVER use Gen-Z slang (ngl, bruh, bet, no cap, fr, lowkey, lmao, etc.)

Output JSON array. Each element has "messages" with role/content. First is system, then alternating user/assistant.

System prompt: {json.dumps(SYS_PROMPT[:1500])}...

Return ONLY the JSON array."""

    try:
        examples = _call_gemini(token, meta_prompt, max_tokens=16384, timeout=90)
        if not isinstance(examples, list):
            return []
        valid = []
        for ex in examples:
            msgs = ex.get("messages", [])
            if len(msgs) >= 4:
                if msgs[0].get("role") != "system":
                    msgs.insert(0, {"role": "system", "content": SYS_PROMPT})
                else:
                    msgs[0]["content"] = SYS_PROMPT
                for m in msgs:
                    if m["role"] == "assistant":
                        c = m["content"]
                        c = c.replace("*", "").replace("—", ",").replace("–", "-")
                        m["content"] = c
                valid.append({"messages": msgs})
        return valid
    except Exception as e:
        print(f"  Error generating multi-turn: {e}", file=sys.stderr)
        return []


def main():
    output_dir = os.path.join(os.path.dirname(__file__), '..', 'training-data')
    output_file = os.path.join(output_dir, 'synthetic-v8-gemini.jsonl')

    print("Generating training data at scale using Gemini 3.1 Flash Lite...")
    print(f"  {len(SCENARIOS)} scenarios x ~10 examples each = ~{len(SCENARIOS)*10} single-turn")
    print(f"  + multi-turn batches")
    print(f"  Output: {output_file}")
    print()

    token = get_access_token()
    all_examples = []

    # Phase 1: Single/short-turn examples from each scenario
    for i, (name, desc) in enumerate(SCENARIOS):
        print(f"[{i+1}/{len(SCENARIOS)}] {name}...", end=" ", flush=True)
        examples = generate_batch(token, name, desc, count=10)
        print(f"{len(examples)} examples")
        all_examples.extend(examples)

        if (i + 1) % 15 == 0:
            token = get_access_token()
            time.sleep(1)
        else:
            time.sleep(0.3)

    # Phase 2: Multi-turn conversations in batches
    print("\nGenerating multi-turn conversations...")
    for batch in range(5):
        print(f"  Multi-turn batch {batch+1}/5...", end=" ", flush=True)
        if batch % 2 == 0:
            token = get_access_token()
        examples = generate_multi_turn(token, count=20)
        print(f"{len(examples)} conversations")
        all_examples.extend(examples)
        time.sleep(1)

    # Phase 3: Quality check — strip any remaining markdown/formal patterns
    cleaned = 0
    for ex in all_examples:
        for m in ex.get("messages", []):
            if m["role"] == "assistant":
                orig = m["content"]
                c = orig
                c = c.replace("**", "").replace("*", "")
                c = c.replace("—", ",").replace("–", "-")
                if c.startswith("I ") or c.startswith("I'm"):
                    pass  # ok
                if c != orig:
                    cleaned += 1
                    m["content"] = c

    print(f"\nCleaned {cleaned} assistant messages of remaining markdown/formal patterns")
    print(f"Total examples generated: {len(all_examples)}")

    # Write output
    with open(output_file, 'w') as f:
        for ex in all_examples:
            f.write(json.dumps(ex) + '\n')

    print(f"Written to {output_file}")

    # Stats
    single = sum(1 for ex in all_examples if len(ex.get("messages", [])) <= 4)
    multi = sum(1 for ex in all_examples if len(ex.get("messages", [])) > 4)
    print(f"  Single/short-turn: {single}")
    print(f"  Multi-turn: {multi}")

if __name__ == "__main__":
    main()
