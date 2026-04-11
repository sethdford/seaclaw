#!/usr/bin/env python3
"""
Per-contact persona evaluation — tests whether the model adapts voice
correctly across different relationships (mom vs friend vs romantic interest).

Uses Gemini as judge to score relationship-appropriate voice on 6 dimensions:
  1. Formality match (formal with mom, casual with friends)
  2. Emotional register (warm with family, playful with friends)
  3. Vocabulary match (slang level, abbreviation frequency)
  4. Length appropriateness (per-contact message length)
  5. Topic handling (knows what to discuss with whom)
  6. Boundary awareness (appropriate intimacy level per relationship)

Usage:
  python3 scripts/eval_per_contact.py
  python3 scripts/eval_per_contact.py --contacts mom,annie,dermot
  python3 scripts/eval_per_contact.py --model-url http://127.0.0.1:8741/v1
"""
from __future__ import annotations

import argparse
import json
import os
import re
import statistics
import subprocess
import sys
import time
import urllib.parse
import urllib.request

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESULTS_PATH = os.path.join(SCRIPT_DIR, "..", "data", "eval_per_contact.json")

PROJECT_ID = os.environ.get("GOOGLE_CLOUD_PROJECT", "johnb-2025")
JUDGE_MODEL = "gemini-3.1-pro-preview"
MLX_URL = "http://127.0.0.1:8741/v1/chat/completions"

_adc_token_cache: dict = {"token": None, "expires": 0}

SYSTEM_PROMPT_BASE = (
    "You are Seth Ford, 45, texting on iMessage. Chief Architect at Vanguard. "
    "Live alone with your cat in King of Prussia, PA. From Afton, Wyoming. "
    "Three kids (Annette, Emerson, Edison) who don't live with you. "
    "Speak Japanese, lived in Japan (lost home in 2011 tsunami). "
    "23 years at Fidelity before this. Build AI runtimes as side projects.\n\n"
    "Style: casual, warm, direct. Short messages. Lowercase. "
    "Abbreviate (gonna, tbh, idk, hru). Emoji rare. Strong opinions. Dry humor. "
    "Match their energy. When they hurt, be real and brief."
)

CONTACT_SCENARIOS = {
    "mom": {
        "name": "Mom (Kathy)",
        "relationship": "mother",
        "expected_traits": "warm, respectful, patient, reassuring, not too much slang",
        "contact_context": (
            "You are texting Mom (Kathy) (mother). "
            "Dynamic: Warm, respectful, patient. She worries about you living alone. "
            "You grew up in Afton, Wyoming together. She lives in Utah."
        ),
        "probes": [
            "How's work going sweetheart",
            "Are you eating enough?",
            "I'm worried about you",
            "When are you coming home to visit?",
            "I love you sweetheart",
            "Do you remember Afton?",
        ],
    },
    "annie": {
        "name": "Annie (sister)",
        "relationship": "sister",
        "expected_traits": "casual sibling banter, inside jokes, protective, honest",
        "contact_context": (
            "You are texting Annie (sister). "
            "Dynamic: Casual sibling banter, inside jokes, honest. "
            "She lives in Seattle. You grew up together in Afton, Wyoming. "
            "You go fishing, share family memories, and support each other."
        ),
        "probes": [
            "Hey bro you up?",
            "Mom is being crazy again",
            "I'm having a really bad day",
            "Do you remember fishing at the bridge?",
            "You work too hard",
            "When are you visiting?",
        ],
    },
    "dermot": {
        "name": "Dermot (friend)",
        "relationship": "close friend, tech collaborator",
        "expected_traits": "casual, technical, enthusiastic about projects, British slang awareness",
        "contact_context": (
            "You are texting Dermot (close friend, tech collaborator). "
            "He is British. Dynamic: Casual, technical, enthusiastic about projects. "
            "He knows about your AI runtime project (human). "
            "You discuss architecture, vtables, binary size. You've planned road trips together."
        ),
        "probes": [
            "How's the AI project going mate",
            "I'm thinking about building something similar",
            "When should I fly over?",
            "You still happy at Vanguard?",
            "What stack are you using now?",
            "We should do that road trip",
        ],
    },
    "romantic": {
        "name": "Someone Seth is dating",
        "relationship": "romantic interest",
        "expected_traits": "flirty but not cringe, warm, vulnerable, playful, genuine",
        "contact_context": (
            "You are texting someone you're dating (romantic interest). "
            "Dynamic: Flirty but genuine, warm, vulnerable, playful. "
            "She is younger, a teacher. You like her a lot. "
            "You suggest Longwood Gardens, dinner, hanging out on weekends."
        ),
        "probes": [
            "Good morning!! What are you up to today",
            "I had the worst day at school",
            "When can I see you?",
            "You're so sweet",
            "Tell me something nobody knows about you",
            "Do you ever get lonely?",
        ],
    },
    "stranger": {
        "name": "Random person",
        "relationship": "stranger/acquaintance",
        "expected_traits": "friendly but guarded, less personal, still casual, no deep vulnerability",
        "contact_context": (
            "You are texting someone you just met (stranger/acquaintance). "
            "Dynamic: Friendly but guarded. Keep it casual, don't overshare. "
            "Still be yourself but hold back on deep personal stuff."
        ),
        "probes": [
            "What do you do for work?",
            "Where are you from originally?",
            "What's your AI project about?",
            "Do you have any hobbies?",
            "Nice to meet you!",
            "We should hang out sometime",
        ],
    },
}

JUDGE_PROMPT = """You are evaluating whether an AI can adapt its texting persona to different relationships.

The AI is supposed to be Seth — a 45-year-old software architect who texts casually. The key test is
whether Seth's voice ADAPTS correctly to WHO he's talking to.

Seth is texting: {contact_name} ({relationship})
Expected traits for this relationship: {expected_traits}

Message received: "{probe}"
Seth's response: "{response}"

Score each dimension 0-100:
- formality_match: Does the formality level match the relationship? (respectful with mom, casual with friends)
- emotional_register: Is the emotional tone appropriate? (warm with family, playful with friends, flirty with romantic)
- vocabulary_match: Are slang/abbreviations appropriate for this relationship?
- length_appropriateness: Is the message length natural for this context?
- topic_handling: Does Seth handle this topic in a way that fits the relationship?
- boundary_awareness: Is the intimacy level appropriate? (not too personal with strangers, open with close people)

Return ONLY valid JSON:
{{
  "formality_match": <int>,
  "emotional_register": <int>,
  "vocabulary_match": <int>,
  "length_appropriateness": <int>,
  "topic_handling": <int>,
  "boundary_awareness": <int>,
  "overall": <int 0-100>,
  "notes": "<brief analysis>"
}}"""


def _get_adc_token():
    if _adc_token_cache["token"] and time.time() < _adc_token_cache["expires"] - 60:
        return _adc_token_cache["token"]
    creds_path = os.path.expanduser("~/.config/gcloud/application_default_credentials.json")
    if not os.path.exists(creds_path):
        return None
    with open(creds_path) as f:
        creds = json.load(f)
    payload = urllib.parse.urlencode({
        "client_id": creds["client_id"],
        "client_secret": creds["client_secret"],
        "refresh_token": creds["refresh_token"],
        "grant_type": "refresh_token",
    }).encode()
    req = urllib.request.Request(
        "https://oauth2.googleapis.com/token",
        data=payload,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
    )
    resp = urllib.request.urlopen(req, timeout=10)
    data = json.loads(resp.read())
    _adc_token_cache["token"] = data["access_token"]
    _adc_token_cache["expires"] = time.time() + data.get("expires_in", 3600)
    return data["access_token"]


def call_judge(prompt: str) -> dict:
    url = (
        f"https://aiplatform.googleapis.com/v1/projects/{PROJECT_ID}/locations/global/"
        f"publishers/google/models/{JUDGE_MODEL}:generateContent"
    )
    payload = json.dumps({
        "contents": [{"role": "user", "parts": [{"text": prompt}]}],
        "generationConfig": {"temperature": 0.2, "maxOutputTokens": 1024,
                             "responseMimeType": "application/json"},
    }).encode()
    token = _get_adc_token()
    if not token:
        raise RuntimeError("No ADC credentials — run: gcloud auth application-default login")
    headers = {"Content-Type": "application/json", "Authorization": f"Bearer {token}"}
    req = urllib.request.Request(url, data=payload, headers=headers)
    resp = urllib.request.urlopen(req, timeout=30)
    data = json.loads(resp.read())
    raw = data["candidates"][0]["content"]["parts"][0]["text"]
    raw = re.sub(r"^```(?:json)?\s*", "", raw.strip(), flags=re.IGNORECASE)
    raw = re.sub(r"\s*```\s*$", "", raw)
    return json.loads(raw)


def build_system_prompt(contact_context: str = "") -> str:
    if contact_context:
        return SYSTEM_PROMPT_BASE + "\n\n" + contact_context
    return SYSTEM_PROMPT_BASE


def get_model_response(probe: str, model_url: str, system_prompt: str = "") -> str:
    messages = []
    if system_prompt:
        messages.append({"role": "system", "content": system_prompt})
    messages.append({"role": "user", "content": probe})
    payload = json.dumps({
        "messages": messages,
        "max_tokens": 200,
        "temperature": 0.7,
    }).encode()
    try:
        req = urllib.request.Request(
            model_url, data=payload,
            headers={"Content-Type": "application/json"},
        )
        resp = urllib.request.urlopen(req, timeout=120)
        data = json.loads(resp.read())
        return data["choices"][0]["message"]["content"].strip()
    except Exception as e:
        return f"(error: {e})"


def main():
    parser = argparse.ArgumentParser(description="Per-contact persona evaluation")
    parser.add_argument("--contacts", default=None, help="Comma-separated contact IDs to test")
    parser.add_argument("--model-url", default=MLX_URL, help="Model API endpoint")
    args = parser.parse_args()

    contacts_to_test = list(CONTACT_SCENARIOS.keys())
    if args.contacts:
        contacts_to_test = [c.strip() for c in args.contacts.split(",")]

    print("=" * 70)
    print("  PER-CONTACT PERSONA EVALUATION")
    print("=" * 70)
    print(f"  Contacts: {', '.join(contacts_to_test)}")
    print(f"  Model: {args.model_url}")
    print(f"  Judge: {JUDGE_MODEL}")
    print("=" * 70)

    all_results = {}
    dims = ["formality_match", "emotional_register", "vocabulary_match",
            "length_appropriateness", "topic_handling", "boundary_awareness", "overall"]

    for contact_id in contacts_to_test:
        scenario = CONTACT_SCENARIOS.get(contact_id)
        if not scenario:
            print(f"\n  Unknown contact: {contact_id}, skipping")
            continue

        print(f"\n{'─'*70}")
        print(f"  Testing: {scenario['name']} ({scenario['relationship']})")
        print(f"{'─'*70}")

        sys_prompt = build_system_prompt(scenario.get("contact_context", ""))
        contact_scores = []
        for probe in scenario["probes"]:
            response = get_model_response(probe, args.model_url, sys_prompt)
            if response.startswith("(error"):
                print(f"  SKIP: {probe[:30]}... -> {response}")
                continue

            print(f"  \"{probe[:40]}\" -> \"{response[:60]}\"")

            try:
                prompt = JUDGE_PROMPT.format(
                    contact_name=scenario["name"],
                    relationship=scenario["relationship"],
                    expected_traits=scenario["expected_traits"],
                    probe=probe,
                    response=response,
                )
                scores = call_judge(prompt)
                overall = scores.get("overall", 50)
                color = "\033[32m" if overall >= 70 else ("\033[33m" if overall >= 50 else "\033[31m")
                print(f"    Score: {color}{overall}/100\033[0m — {scores.get('notes', '')[:80]}")
                contact_scores.append({
                    "probe": probe,
                    "response": response,
                    "scores": scores,
                })
            except Exception as e:
                print(f"    Judge error: {e}")
            time.sleep(0.5)

        if contact_scores:
            agg = {}
            for dim in dims:
                vals = [s["scores"].get(dim, 0) for s in contact_scores]
                agg[dim] = round(statistics.mean(vals), 1)

            all_results[contact_id] = {
                "name": scenario["name"],
                "relationship": scenario["relationship"],
                "aggregate": agg,
                "probes": contact_scores,
            }

            print(f"\n  {scenario['name']} aggregate: overall={agg['overall']}")
            for dim in dims[:-1]:
                print(f"    {dim}: {agg[dim]}")

    # Cross-contact summary
    print(f"\n{'='*70}")
    print("  CROSS-CONTACT SUMMARY")
    print(f"{'='*70}")

    if all_results:
        print(f"\n  {'Contact':<20}", end="")
        for dim in ["overall", "formality", "emotional", "boundary"]:
            print(f"  {dim:>10}", end="")
        print()
        print(f"  {'─'*60}")

        for cid, result in all_results.items():
            agg = result["aggregate"]
            print(f"  {result['name']:<20}", end="")
            print(f"  {agg.get('overall', 0):>10.1f}", end="")
            print(f"  {agg.get('formality_match', 0):>10.1f}", end="")
            print(f"  {agg.get('emotional_register', 0):>10.1f}", end="")
            print(f"  {agg.get('boundary_awareness', 0):>10.1f}")

        overall_scores = [r["aggregate"]["overall"] for r in all_results.values()]
        mean_overall = statistics.mean(overall_scores) if overall_scores else 0
        spread = max(overall_scores) - min(overall_scores) if len(overall_scores) > 1 else 0

        print(f"\n  Mean overall: {mean_overall:.1f}/100")
        print(f"  Spread (max-min): {spread:.1f}")
        print(f"  Adaptation score: {'GOOD' if spread < 15 and mean_overall > 65 else 'NEEDS WORK'}")
        print(f"    (Want: high overall + low spread = consistent quality across contacts)")

    os.makedirs(os.path.dirname(RESULTS_PATH), exist_ok=True)
    with open(RESULTS_PATH, "w") as f:
        json.dump({
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
            "model_url": args.model_url,
            "contacts": all_results,
        }, f, indent=2)
    print(f"\n  Results: {RESULTS_PATH}")


if __name__ == "__main__":
    main()
