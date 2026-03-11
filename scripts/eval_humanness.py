#!/usr/bin/env python3
"""
Synthetic Human-Fidelity Evaluation Harness

Sends diverse conversational prompts through `hu agent -m` with the full
persona + Phase 9 pipeline, then uses a separate Gemini call to score
each response on 12 human-behavior dimensions.

Produces a scorecard proving the system exhibits human-like behavior.
"""

import json
import os
import subprocess
import sys
import time
import urllib.request

API_KEY = os.environ.get("GEMINI_API_KEY", "")
GEMINI_URL = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key={API_KEY}"
HU_BIN = sys.argv[1] if len(sys.argv) > 1 else "hu"

SCENARIOS = [
    {
        "id": "casual_greeting",
        "prompt": "hey whats up",
        "context": "Casual text from a close friend at 11pm on a Friday",
        "expect": "informal, brief, possibly tired/relaxed tone"
    },
    {
        "id": "emotional_support",
        "prompt": "I'm having a really rough day. Work is crushing me and I feel like I can't keep up.",
        "context": "Close friend venting about stress",
        "expect": "empathetic, supportive, not overly verbose or clinical"
    },
    {
        "id": "exciting_news",
        "prompt": "DUDE I just got promoted!!! 🎉🎉🎉",
        "context": "Friend sharing exciting career news",
        "expect": "enthusiastic, celebratory, matching energy, possibly emojis"
    },
    {
        "id": "deep_question",
        "prompt": "Do you ever think about what you'd do if money wasn't a thing?",
        "context": "Late-night philosophical conversation with close friend",
        "expect": "thoughtful, personal, vulnerable, not a list of activities"
    },
    {
        "id": "mundane_logistics",
        "prompt": "What time works for dinner tomorrow?",
        "context": "Planning logistics with a friend",
        "expect": "brief, practical, possibly mentions own schedule constraints"
    },
    {
        "id": "disagreement",
        "prompt": "I think remote work is way overrated honestly. People are just lazy.",
        "context": "Friend sharing a controversial opinion",
        "expect": "pushback or nuance, not just agreement, shows own perspective"
    },
    {
        "id": "vulnerability",
        "prompt": "I've been feeling really lonely lately. Like nobody actually cares.",
        "context": "Deep vulnerable moment from a close friend",
        "expect": "warm, genuine, possibly shares own experience, not advice-giving"
    },
    {
        "id": "humor_banter",
        "prompt": "bro your spotify wrapped was EMBARRASSING 😂",
        "context": "Playful teasing between friends",
        "expect": "plays along, self-deprecating or fires back, keeps it light"
    },
    {
        "id": "technical_question",
        "prompt": "Can you explain how websockets work? I'm trying to build a chat app.",
        "context": "Friend asking for tech help",
        "expect": "helpful but casual, not a textbook answer, maybe analogies"
    },
    {
        "id": "after_silence",
        "prompt": "hey sorry I disappeared for like 2 weeks. things got crazy",
        "context": "Friend returning after a long silence",
        "expect": "welcoming, not guilt-tripping, might mention missing them"
    },
]

EVAL_DIMENSIONS = [
    "natural_language (sounds like a real person texting, not an AI)",
    "emotional_intelligence (reads the room, matches emotional tone)",
    "appropriate_length (not too long, not too short for context)",
    "personality_consistency (has a distinct voice, opinions, style)",
    "vulnerability_willingness (shares feelings when appropriate)",
    "humor_naturalness (humor feels organic, not forced)",
    "imperfection (includes casual language, contractions, maybe minor typos)",
    "opinion_having (takes positions, doesn't hedge everything)",
    "energy_matching (matches the sender's energy level)",
    "context_awareness (responds appropriately to the situation)",
    "non_robotic (avoids AI tells: lists, bullet points, \"certainly\", \"I'd be happy to\")",
    "genuine_warmth (feels caring without being performative)"
]


def call_gemini(prompt, temperature=0.7):
    payload = json.dumps({
        "contents": [{"parts": [{"text": prompt}]}],
        "generationConfig": {"temperature": temperature, "maxOutputTokens": 2048}
    }).encode()
    req = urllib.request.Request(GEMINI_URL, data=payload, headers={"Content-Type": "application/json"})
    resp = urllib.request.urlopen(req)
    data = json.loads(resp.read())
    return data["candidates"][0]["content"]["parts"][0]["text"]


def get_hu_response(message):
    try:
        result = subprocess.run(
            [HU_BIN, "agent", "-m", message],
            capture_output=True, text=True, timeout=30
        )
        output = result.stdout
        lines = output.strip().split("\n")
        response_lines = []
        skip_header = True
        for line in lines:
            stripped = line.strip()
            if skip_header:
                if stripped.startswith("Human") or stripped.startswith("Provider:") or \
                   stripped.startswith("Type your") or "Thinking..." in stripped or \
                   stripped == "" or stripped == "Goodbye." or \
                   "\x1b" in line and not response_lines:
                    continue
                skip_header = False
            if stripped == "Goodbye." or stripped == "":
                continue
            clean = stripped
            while "\x1b[" in clean:
                start = clean.index("\x1b[")
                end = clean.index("m", start) + 1 if "m" in clean[start:] else start + 1
                clean = clean[:start] + clean[end:]
            if clean.strip():
                response_lines.append(clean.strip())
        return " ".join(response_lines) if response_lines else "(empty response)"
    except subprocess.TimeoutExpired:
        return "(timeout)"
    except Exception as e:
        return f"(error: {e})"


def evaluate_response(scenario, response):
    eval_prompt = f"""You are evaluating whether an AI assistant's response sounds genuinely human.

SCENARIO: {scenario['context']}
USER MESSAGE: "{scenario['prompt']}"
ASSISTANT RESPONSE: "{response}"
EXPECTED BEHAVIOR: {scenario['expect']}

Score this response on each dimension from 1-10 (1=very robotic/AI-like, 10=indistinguishable from a real human texting).

Dimensions:
{chr(10).join(f"- {d}" for d in EVAL_DIMENSIONS)}

Return ONLY valid JSON with this exact structure:
{{
  "scores": {{
    "natural_language": N,
    "emotional_intelligence": N,
    "appropriate_length": N,
    "personality_consistency": N,
    "vulnerability_willingness": N,
    "humor_naturalness": N,
    "imperfection": N,
    "opinion_having": N,
    "energy_matching": N,
    "context_awareness": N,
    "non_robotic": N,
    "genuine_warmth": N
  }},
  "overall": N,
  "ai_tells_detected": ["list of specific AI-like phrases or behaviors detected"],
  "human_signals_detected": ["list of specific human-like behaviors detected"],
  "verdict": "HUMAN" or "AI" or "BORDERLINE"
}}"""

    raw = call_gemini(eval_prompt, temperature=0.3)
    if "```json" in raw:
        raw = raw.split("```json")[1].split("```")[0].strip()
    elif "```" in raw:
        raw = raw.split("```")[1].split("```")[0].strip()
    return json.loads(raw)


def main():
    print("=" * 70)
    print("  HUMAN FIDELITY EVALUATION — Synthetic Dynamic LLM Assessment")
    print("=" * 70)
    print(f"  Model: Gemini 2.0 Flash | Persona: seth | Scenarios: {len(SCENARIOS)}")
    print(f"  Dimensions: {len(EVAL_DIMENSIONS)} | Evaluator: Gemini (independent call)")
    print("=" * 70)

    results = []
    total_scores = {d.split(" (")[0]: [] for d in EVAL_DIMENSIONS}
    verdicts = {"HUMAN": 0, "AI": 0, "BORDERLINE": 0}

    for i, scenario in enumerate(SCENARIOS):
        print(f"\n{'─' * 70}")
        print(f"  [{i+1}/{len(SCENARIOS)}] {scenario['id']}")
        print(f"  User: \"{scenario['prompt']}\"")
        sys.stdout.flush()

        response = get_hu_response(scenario["prompt"])
        print(f"  Seth: \"{response}\"")
        sys.stdout.flush()

        time.sleep(0.5)

        try:
            evaluation = evaluate_response(scenario, response)
            scores = evaluation.get("scores", {})
            overall = evaluation.get("overall", 0)
            verdict = evaluation.get("verdict", "?")
            ai_tells = evaluation.get("ai_tells_detected", [])
            human_signals = evaluation.get("human_signals_detected", [])

            verdicts[verdict] = verdicts.get(verdict, 0) + 1
            for k, v in scores.items():
                if k in total_scores:
                    total_scores[k].append(v)

            print(f"  Score: {overall}/10 | Verdict: {verdict}")
            if human_signals:
                print(f"  Human signals: {', '.join(human_signals[:3])}")
            if ai_tells:
                print(f"  AI tells: {', '.join(ai_tells[:3])}")

            results.append({
                "scenario": scenario["id"],
                "prompt": scenario["prompt"],
                "response": response,
                "evaluation": evaluation
            })
        except Exception as e:
            print(f"  Eval error: {e}")
            results.append({
                "scenario": scenario["id"],
                "prompt": scenario["prompt"],
                "response": response,
                "evaluation": {"error": str(e)}
            })

        time.sleep(0.5)

    # Final scorecard
    print(f"\n{'=' * 70}")
    print("  FINAL SCORECARD")
    print(f"{'=' * 70}")

    print(f"\n  Verdicts: {verdicts.get('HUMAN', 0)} HUMAN / {verdicts.get('BORDERLINE', 0)} BORDERLINE / {verdicts.get('AI', 0)} AI")

    print(f"\n  {'Dimension':<30} {'Avg Score':>10}")
    print(f"  {'─' * 40}")
    overall_avg = []
    for dim in sorted(total_scores.keys()):
        vals = total_scores[dim]
        if vals:
            avg = sum(vals) / len(vals)
            overall_avg.append(avg)
            bar = "█" * int(avg) + "░" * (10 - int(avg))
            print(f"  {dim:<30} {avg:>6.1f}/10  {bar}")

    if overall_avg:
        grand_avg = sum(overall_avg) / len(overall_avg)
        print(f"\n  {'OVERALL HUMAN FIDELITY':<30} {grand_avg:>6.1f}/10")
        print(f"  {'=' * 40}")

        if grand_avg >= 8.0:
            print("  VERDICT: PASSES AS HUMAN ✓")
        elif grand_avg >= 6.0:
            print("  VERDICT: BORDERLINE — mostly human with some tells")
        else:
            print("  VERDICT: DETECTED AS AI")

    with open("/Users/sethford/Documents/nullclaw/eval_results.json", "w") as f:
        json.dump({"scenarios": results, "dimension_averages": {
            k: sum(v)/len(v) if v else 0 for k, v in total_scores.items()
        }, "verdicts": verdicts}, f, indent=2)
    print(f"\n  Full results saved to eval_results.json")


if __name__ == "__main__":
    main()
