#!/usr/bin/env python3
"""
Conversation Trainer — LLM-driven long conversation with h-uman for training data collection.

Uses Gemini (Vertex AI) to simulate a human having a real, multi-turn conversation with
h-uman via the CLI. Scores every response and captures data for the full ML training pipeline:
DPO pairs, persona feedback, and conversation logs for local model training.

Requires:
  - Built binary: build/human (override with --human-bin or HUMAN_BIN)
  - A reachable provider for `human agent` (HUMAN_PROVIDER / HUMAN_MODEL plus API keys)
  - ADC credentials for Vertex AI Gemini (~/.config/gcloud/application_default_credentials.json)

Usage:
  python3 scripts/convo-trainer.py --turns 20 --scenario mixed --verbose
  python3 scripts/convo-trainer.py --turns 5 --dry-run
  python3 scripts/convo-trainer.py --turns 30 --scenario emotional_support --output training-run-1/
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sqlite3
import subprocess
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_HUMAN = REPO_ROOT / "build" / "human"

PROJECT_ID = os.environ.get("GOOGLE_CLOUD_PROJECT", "johnb-2025")
GEMINI_MODEL = os.environ.get("CONVO_TRAINER_MODEL", "gemini-3.1-pro-preview")
TIMEOUT_SEC = int(os.environ.get("CONVO_TRAINER_TIMEOUT", "120"))

_adc_token_cache: dict[str, Any] = {"token": None, "expires": 0}

# ---------------------------------------------------------------------------
# Gemini (Vertex AI) helpers — same pattern as eval_multiturn.py
# ---------------------------------------------------------------------------

def _get_adc_token() -> str | None:
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


def _gemini_url() -> str:
    return (
        f"https://aiplatform.googleapis.com/v1/projects/{PROJECT_ID}/locations/global/"
        f"publishers/google/models/{GEMINI_MODEL}:generateContent"
    )


def call_gemini(prompt: str, *, temperature: float = 0.7, json_mode: bool = True) -> str:
    gen_config: dict[str, Any] = {
        "temperature": temperature,
        "maxOutputTokens": 4096,
    }
    if json_mode:
        gen_config["responseMimeType"] = "application/json"
    payload = json.dumps({
        "contents": [{"role": "user", "parts": [{"text": prompt}]}],
        "generationConfig": gen_config,
    }).encode()
    token = _get_adc_token()
    if not token:
        raise RuntimeError("No ADC credentials — run: gcloud auth application-default login")
    headers = {"Content-Type": "application/json", "Authorization": f"Bearer {token}"}
    req = urllib.request.Request(_gemini_url(), data=payload, headers=headers)
    resp = urllib.request.urlopen(req, timeout=60)
    data = json.loads(resp.read())
    return data["candidates"][0]["content"]["parts"][0]["text"]


def _strip_json_fence(text: str) -> str:
    t = text.strip()
    if t.startswith("```"):
        t = re.sub(r"^```(?:json)?\s*", "", t, flags=re.IGNORECASE)
        t = re.sub(r"\s*```\s*$", "", t)
    return t.strip()


# ---------------------------------------------------------------------------
# h-uman CLI interaction
# ---------------------------------------------------------------------------

def run_human(
    human_bin: Path,
    message: str,
    session_id: str,
    timeout_sec: int = TIMEOUT_SEC,
    persona: str | None = None,
    provider: str | None = None,
    model: str | None = None,
    iso_home: str | None = None,
) -> tuple[int, str, str]:
    """Run `human agent -m <message> --session <id>`, return (exit_code, stdout, stderr)."""
    env = os.environ.copy()
    if persona:
        env["HUMAN_PERSONA"] = persona
    if provider:
        env["HUMAN_PROVIDER"] = provider
    if model:
        env["HUMAN_MODEL"] = model
    if iso_home:
        env["HOME"] = iso_home
    try:
        r = subprocess.run(
            [str(human_bin), "agent", "-m", message, "--session", session_id],
            capture_output=True,
            text=True,
            timeout=timeout_sec,
            env=env,
            cwd=str(REPO_ROOT),
        )
        return r.returncode, (r.stdout or "").strip(), (r.stderr or "").strip()
    except subprocess.TimeoutExpired as e:
        out = e.stdout if isinstance(e.stdout, str) else (
            e.stdout.decode("utf-8", errors="replace") if e.stdout else "")
        err = e.stderr if isinstance(e.stderr, str) else (
            e.stderr.decode("utf-8", errors="replace") if e.stderr else "")
        return 124, (out or "").strip(), (err or "").strip()


def clean_response(raw: str) -> str:
    """Strip CLI chrome (banner, prompt markers, goodbye, updater) from agent stdout."""
    lines = raw.splitlines()
    out: list[str] = []
    for line in lines:
        if any(skip in line for skip in [
            "Human v", "Provider:", "Type your", "Goodbye", "[human]",
            "Update available", "Downloading", "Development installation",
            "To update", "git pull", "cmake --build", "[agent_stream",
            "[config]", "consistency drift",
        ]):
            continue
        cleaned = re.sub(r"^\x1b\[[0-9;]*m", "", line)
        cleaned = re.sub(r"\x1b\[[0-9;]*m", "", cleaned)
        cleaned = re.sub(r"^> ", "", cleaned)
        if cleaned.strip():
            out.append(cleaned)
    return "\n".join(out).strip()


# ---------------------------------------------------------------------------
# Scenario profiles
# ---------------------------------------------------------------------------

SCENARIOS: dict[str, dict[str, str]] = {
    "casual_hangout": {
        "persona": (
            "You are a casual friend in your 30s texting your buddy. You talk about "
            "weekend plans, food, TV shows, sports, and life. Keep it natural — use "
            "lowercase, abbreviations, slang. Ask questions, share opinions, be playful."
        ),
        "opener": "hey whats up, haven't talked in a minute",
    },
    "emotional_support": {
        "persona": (
            "You are someone going through a tough time. Start light but gradually open "
            "up about feeling stuck in life, loneliness, or work burnout. Be real and "
            "vulnerable. You want someone to listen, not fix things."
        ),
        "opener": "hey... you around?",
    },
    "technical_collab": {
        "persona": (
            "You are a software developer working on a project. You need help debugging, "
            "brainstorming architecture, or talking through a tricky problem. Be specific "
            "with code details. Ask follow-ups. Share your thinking."
        ),
        "opener": "hey, got a sec? I'm stuck on something",
    },
    "philosophical": {
        "persona": (
            "You are someone who likes thinking about big questions — consciousness, "
            "free will, the meaning of life, ethics of technology. Start casually but go "
            "deep. Share your own takes and push back on ideas you disagree with."
        ),
        "opener": "random question — do you think we actually have free will?",
    },
    "daily_life": {
        "persona": (
            "You are a friend sharing mundane life updates — cooking, errands, kids, "
            "weather, commute stories. Keep it low-key and relatable. React to what "
            "the other person says. Mix in small complaints and small joys."
        ),
        "opener": "dude I just spent 45 minutes at the DMV and I want to scream",
    },
    "mixed": {
        "persona": (
            "You are a close friend having a natural, wide-ranging conversation. Topics "
            "shift organically — one minute you're joking about something, the next you're "
            "talking about something real. You're genuine, curious, and unpredictable. Mix "
            "humor, opinions, personal stuff, and occasional deep moments."
        ),
        "opener": "yo what's good",
    },
}

# ---------------------------------------------------------------------------
# Gemini prompts for simulation and scoring
# ---------------------------------------------------------------------------

SIMULATOR_SYSTEM = """You are role-playing as a person texting a friend. Stay in character.

Your persona: {persona}

Conversation so far:
{history}

The friend just said: {last_response}

Generate your next message. Requirements:
- Stay in character — casual texting style, lowercase, natural
- Build on what was said, don't repeat yourself
- Vary length — sometimes short, sometimes a paragraph
- Don't be a generic chatbot — have opinions, reactions, personality
- After {turns_remaining} more turns, start wrapping up naturally

Return JSON: {{"message": "your next message"}}"""

SCORER_SYSTEM = """You are evaluating a conversational AI response for quality. The AI is supposed
to sound like a real human friend, not a chatbot.

Conversation context:
{history}

User message: {user_message}
AI response: {ai_response}

Score each dimension 0-100:
- naturalness: Does it sound like a real person texting? No AI markers, natural rhythm.
- persona_consistency: Does it maintain a consistent personality and voice across the conversation?
- engagement: Does it ask questions, react genuinely, keep the conversation going?
- emotional_intelligence: Does it read the emotional tone correctly and respond appropriately?
- substance: Is there actual content — opinions, stories, facts — not just filler?

If the response scores below 60 on naturalness or persona_consistency, also write a "correction" —
what a real person with this personality would have said instead. Otherwise set correction to null.

Return JSON:
{{
  "naturalness": <int>,
  "persona_consistency": <int>,
  "engagement": <int>,
  "emotional_intelligence": <int>,
  "substance": <int>,
  "overall": <int 0-100 weighted average>,
  "correction": <string or null>,
  "notes": "<brief explanation of scoring>"
}}"""

# ---------------------------------------------------------------------------
# Data writers
# ---------------------------------------------------------------------------

def init_chat_db(path: Path) -> sqlite3.Connection:
    """Create SQLite conversation log compatible with prepare-conversations."""
    conn = sqlite3.connect(str(path))
    conn.execute(
        "CREATE TABLE IF NOT EXISTS message("
        "ROWID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "is_from_me INTEGER, "
        "text TEXT, "
        "date INTEGER)"
    )
    conn.commit()
    return conn


def write_chat_message(conn: sqlite3.Connection, is_from_me: int, text: str, ts: int) -> None:
    conn.execute(
        "INSERT INTO message(is_from_me, text, date) VALUES(?, ?, ?)",
        (is_from_me, text, ts),
    )
    conn.commit()


def init_dpo_db(path: Path) -> sqlite3.Connection:
    """Create SQLite DPO pairs table matching hu_dpo_collector_t schema."""
    conn = sqlite3.connect(str(path))
    conn.execute(
        "CREATE TABLE IF NOT EXISTS dpo_pairs("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "prompt TEXT, chosen TEXT, rejected TEXT, "
        "margin REAL, timestamp INTEGER, source TEXT)"
    )
    conn.commit()
    return conn


def write_dpo_pair(
    conn: sqlite3.Connection,
    prompt: str,
    chosen: str,
    rejected: str,
    margin: float,
    ts: int,
) -> None:
    conn.execute(
        "INSERT INTO dpo_pairs(prompt, chosen, rejected, margin, timestamp, source) "
        "VALUES(?, ?, ?, ?, ?, ?)",
        (prompt, chosen, rejected, margin, ts, "convo_trainer"),
    )
    conn.commit()


def write_persona_feedback(
    path: Path,
    original: str,
    corrected: str,
    context: str,
) -> None:
    """Append persona feedback JSONL matching hu_persona_feedback_record format."""
    entry = {
        "channel": "cli",
        "original": original,
        "corrected": corrected,
        "context": context,
        "category": "general",
        "ts": int(time.time()),
    }
    with open(path, "a") as f:
        f.write(json.dumps(entry, ensure_ascii=False) + "\n")


# ---------------------------------------------------------------------------
# Dry-run stubs
# ---------------------------------------------------------------------------

_DRY_TURN = 0

def dry_run_human(message: str) -> str:
    global _DRY_TURN
    _DRY_TURN += 1
    responses = [
        "haha yeah I feel that. what have you been up to lately?",
        "oh nice, that sounds fun. I've been kinda swamped with work stuff tbh",
        "yeah for real. hey did you see that thing about the new restaurant downtown?",
        "lol I know right. we should check it out sometime this week",
        "sounds good to me. thursday work for you?",
    ]
    return responses[(_DRY_TURN - 1) % len(responses)]


def dry_run_gemini_sim(turn: int) -> str:
    messages = [
        "not much, just chilling. you?",
        "oh nice. yeah work has been wild. you watching anything good?",
        "haha I need to check that out. what else is new",
        "yeah same honestly. hey wanna grab food this weekend?",
        "cool, saturday works. where should we go",
        "sounds perfect. I've been craving tacos ngl",
        "lol fair. alright I'll hit you up friday to confirm",
        "bet. talk later!",
    ]
    return messages[turn % len(messages)]


def dry_run_gemini_score() -> dict[str, Any]:
    return {
        "naturalness": 72,
        "persona_consistency": 78,
        "engagement": 75,
        "emotional_intelligence": 70,
        "substance": 65,
        "overall": 72,
        "correction": None,
        "notes": "(dry-run) simulated score",
    }


# ---------------------------------------------------------------------------
# Main conversation loop
# ---------------------------------------------------------------------------

def format_history(turns: list[dict[str, str]], last_n: int = 10) -> str:
    """Format recent conversation turns for Gemini context."""
    recent = turns[-last_n:]
    lines: list[str] = []
    for t in recent:
        lines.append(f"User: {t['user']}")
        lines.append(f"Friend: {t['human']}")
    return "\n".join(lines)


def run_conversation(args: argparse.Namespace) -> dict[str, Any]:
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    scenario = SCENARIOS.get(args.scenario)
    if not scenario:
        print(f"Unknown scenario: {args.scenario}", file=sys.stderr)
        print(f"Available: {', '.join(SCENARIOS.keys())}", file=sys.stderr)
        sys.exit(1)

    session_id = args.session or f"convo-{int(time.time())}"
    human_bin = Path(args.human_bin)

    # Isolated home: prevents real contacts/memories/feeds from leaking into
    # the conversation.  The session DB and persona are still accessible.
    iso_home = output_dir / "home"
    iso_human_dir = iso_home / ".human"
    iso_sessions = iso_human_dir / "sessions"
    iso_sessions.mkdir(parents=True, exist_ok=True)
    # Copy config.json with auto_update disabled so updater doesn't pollute stdout
    real_config = Path.home() / ".human" / "config.json"
    if real_config.exists():
        import shutil
        cfg_text = real_config.read_text()
        cfg_text = cfg_text.replace('"auto_update": "apply"', '"auto_update": "off"')
        cfg_text = cfg_text.replace('"auto_update": "notify"', '"auto_update": "off"')
        (iso_human_dir / "config.json").write_text(cfg_text)
    args._iso_home = str(iso_home)  # stash for run_human

    if not args.dry_run and not human_bin.exists():
        print(f"Error: {human_bin} not found. Run: cmake --build build", file=sys.stderr)
        sys.exit(1)

    chat_db = init_chat_db(output_dir / "chat.db")
    dpo_db = init_dpo_db(output_dir / "dpo_pairs.db")
    feedback_path = output_dir / "persona_feedback.jsonl"

    turns: list[dict[str, Any]] = []
    dpo_count = 0
    feedback_count = 0
    user_message = scenario["opener"]

    print()
    print("=" * 60)
    print("  h-uman Conversation Trainer")
    print("=" * 60)
    print(f"  Scenario:  {args.scenario}")
    print(f"  Turns:     {args.turns}")
    print(f"  Session:   {session_id}")
    print(f"  Output:    {output_dir}")
    print(f"  Persona:   {args.persona or '(default)'}")
    print(f"  Dry run:   {args.dry_run}")
    print(f"  Model:     {GEMINI_MODEL}")
    print("=" * 60)
    print()

    for turn_idx in range(args.turns):
        turn_start = time.time()
        ts = int(turn_start)

        # -- 1. Send user message to h-uman --
        if args.verbose:
            print(f"  [{turn_idx + 1:02d}] User: {user_message}")

        write_chat_message(chat_db, 0, user_message, ts)

        if args.dry_run:
            human_response = dry_run_human(user_message)
            exit_code = 0
        else:
            exit_code, raw_stdout, raw_stderr = run_human(
                human_bin, user_message, session_id, persona=args.persona,
                provider=args.provider, model=args.model,
                iso_home=getattr(args, '_iso_home', None),
            )
            human_response = clean_response(raw_stdout)
            if exit_code != 0 and not human_response:
                human_response = f"(error: exit {exit_code})"
                if args.verbose and raw_stderr:
                    print(f"         stderr: {raw_stderr[:200]}")

        write_chat_message(chat_db, 1, human_response, ts + 1)

        if args.verbose:
            resp_preview = human_response[:120] + ("..." if len(human_response) > 120 else "")
            print(f"         h-uman: {resp_preview}")

        # -- 2. Score the response --
        if args.dry_run:
            scores = dry_run_gemini_score()
        else:
            history_text = format_history(
                [{"user": t["user"], "human": t["human"]} for t in turns]
            )
            scorer_prompt = SCORER_SYSTEM.format(
                history=history_text or "(conversation start)",
                user_message=user_message,
                ai_response=human_response,
            )
            try:
                raw_score = call_gemini(scorer_prompt, temperature=0.2)
                scores = json.loads(_strip_json_fence(raw_score))
            except Exception as e:
                print(f"         [scorer error: {e}]")
                scores = {
                    "naturalness": 50, "persona_consistency": 50,
                    "engagement": 50, "emotional_intelligence": 50,
                    "substance": 50, "overall": 50,
                    "correction": None, "notes": f"scorer error: {e}",
                }

        overall = scores.get("overall", 50)
        correction = scores.get("correction")

        if args.verbose:
            dims = " | ".join(
                f"{k}={scores.get(k, '?')}"
                for k in ["naturalness", "persona_consistency", "engagement",
                          "emotional_intelligence", "substance"]
            )
            color = "\033[32m" if overall >= 70 else ("\033[33m" if overall >= 50 else "\033[31m")
            print(f"         Score: {color}{overall}\033[0m  ({dims})")
            if correction:
                print(f"         Correction: {correction[:100]}...")

        # -- 3. Record DPO pair if below threshold --
        if overall < args.dpo_threshold and correction:
            write_dpo_pair(dpo_db, user_message, correction, human_response, 0.7, ts)
            dpo_count += 1
        elif overall < args.dpo_threshold and not correction:
            # Ask Gemini for a correction specifically
            if not args.dry_run:
                try:
                    corr_prompt = (
                        f"The following is a conversation between friends.\n\n"
                        f"History:\n{format_history([{'user': t['user'], 'human': t['human']} for t in turns])}\n\n"
                        f"User: {user_message}\n\n"
                        f"The friend responded: {human_response}\n\n"
                        f"This response scored {overall}/100 on quality. Write a better response "
                        f"that sounds like a real person texting — casual, genuine, engaging. "
                        f"Return JSON: {{\"correction\": \"the better response\"}}"
                    )
                    raw_corr = call_gemini(corr_prompt, temperature=0.8)
                    corr_data = json.loads(_strip_json_fence(raw_corr))
                    correction = corr_data.get("correction", "")
                    if correction:
                        write_dpo_pair(dpo_db, user_message, correction, human_response, 0.7, ts)
                        dpo_count += 1
                except Exception:
                    pass

        # -- 4. Record persona feedback if voice/naturalness is off --
        nat_score = scores.get("naturalness", 100)
        persona_score = scores.get("persona_consistency", 100)
        if (nat_score < 60 or persona_score < 60) and correction:
            write_persona_feedback(feedback_path, human_response, correction, user_message)
            feedback_count += 1

        # -- 5. Record turn data --
        turn_data = {
            "turn": turn_idx + 1,
            "user": user_message,
            "human": human_response,
            "scores": scores,
            "exit_code": exit_code,
            "elapsed_s": round(time.time() - turn_start, 2),
        }
        turns.append(turn_data)

        if args.verbose:
            print()

        # -- 6. Generate next user message --
        if turn_idx < args.turns - 1:
            if args.dry_run:
                user_message = dry_run_gemini_sim(turn_idx + 1)
            else:
                history_text = format_history(
                    [{"user": t["user"], "human": t["human"]} for t in turns]
                )
                sim_prompt = SIMULATOR_SYSTEM.format(
                    persona=scenario["persona"],
                    history=history_text,
                    last_response=human_response,
                    turns_remaining=args.turns - turn_idx - 1,
                )
                try:
                    raw_sim = call_gemini(sim_prompt, temperature=0.9)
                    sim_data = json.loads(_strip_json_fence(raw_sim))
                    user_message = sim_data.get("message", "haha yeah")
                except Exception as e:
                    print(f"  [simulator error: {e}, using fallback]")
                    user_message = "haha yeah, what else is going on?"

        if not args.verbose:
            progress = "." * (turn_idx + 1) + " " * (args.turns - turn_idx - 1)
            avg = sum(t["scores"].get("overall", 0) for t in turns) / len(turns)
            sys.stdout.write(
                f"\r  [{turn_idx + 1:02d}/{args.turns:02d}] {progress} avg={avg:.0f}"
            )
            sys.stdout.flush()

    if not args.verbose:
        print()
        print()

    chat_db.close()
    dpo_db.close()

    # -- Build report --
    overall_scores = [t["scores"].get("overall", 0) for t in turns]
    dim_names = ["naturalness", "persona_consistency", "engagement",
                 "emotional_intelligence", "substance"]
    report = {
        "scenario": args.scenario,
        "session_id": session_id,
        "turns": len(turns),
        "model": GEMINI_MODEL,
        "dry_run": args.dry_run,
        "aggregate": {
            "overall_mean": round(sum(overall_scores) / len(overall_scores), 1),
            "overall_min": min(overall_scores),
            "overall_max": max(overall_scores),
        },
        "dimensions": {},
        "dpo_pairs_collected": dpo_count,
        "persona_corrections": feedback_count,
        "turn_details": turns,
    }
    for dim in dim_names:
        vals = [t["scores"].get(dim, 0) for t in turns]
        report["dimensions"][dim] = {
            "mean": round(sum(vals) / len(vals), 1),
            "min": min(vals),
            "max": max(vals),
        }

    # Trend: compare first half vs second half
    if len(overall_scores) >= 4:
        mid = len(overall_scores) // 2
        first_half = sum(overall_scores[:mid]) / mid
        second_half = sum(overall_scores[mid:]) / (len(overall_scores) - mid)
        report["aggregate"]["trend"] = round(second_half - first_half, 1)
        report["aggregate"]["trend_direction"] = (
            "improving" if second_half > first_half + 2
            else "declining" if second_half < first_half - 2
            else "stable"
        )

    # Write artifacts
    convo_path = output_dir / "conversation.json"
    with open(convo_path, "w") as f:
        json.dump(turns, f, indent=2, ensure_ascii=False)

    report_path = output_dir / "report.json"
    with open(report_path, "w") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)

    # -- Print summary --
    print("=" * 60)
    print("  Results")
    print("=" * 60)
    avg = report["aggregate"]["overall_mean"]
    color = "\033[32m" if avg >= 70 else ("\033[33m" if avg >= 50 else "\033[31m")
    print(f"  Overall quality:  {color}{avg:.0f}/100\033[0m")
    print(f"  Score range:      {report['aggregate']['overall_min']}-{report['aggregate']['overall_max']}")
    if "trend_direction" in report["aggregate"]:
        trend = report["aggregate"]["trend"]
        tdir = report["aggregate"]["trend_direction"]
        print(f"  Trend:            {tdir} ({'+' if trend > 0 else ''}{trend:.1f})")
    print()
    for dim in dim_names:
        d = report["dimensions"][dim]
        print(f"  {dim:25s}  mean={d['mean']:5.1f}  range={d['min']}-{d['max']}")
    print()
    print(f"  DPO pairs collected:      {dpo_count}")
    print(f"  Persona corrections:      {feedback_count}")
    print()
    print("  Artifacts:")
    print(f"    {convo_path}")
    print(f"    {report_path}")
    print(f"    {output_dir / 'chat.db'}")
    print(f"    {output_dir / 'dpo_pairs.db'}")
    if feedback_count > 0:
        print(f"    {feedback_path}")
    print()
    print(f"  Session ID: {session_id}")
    print(f"  Resume:     python3 scripts/convo-trainer.py --session {session_id} --turns N ...")
    print()
    print("  Next steps:")
    print(f"    human ml prepare-conversations --chat-db {output_dir / 'chat.db'} --output training-data/")
    print(f"    human ml dpo-train --db {output_dir / 'dpo_pairs.db'}")
    if feedback_count > 0:
        print(f"    cp {feedback_path} ~/.human/personas/feedback/<name>.jsonl")
    print("=" * 60)
    print()

    return report


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="LLM-driven conversation trainer for h-uman",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Scenarios: " + ", ".join(SCENARIOS.keys()) + "\n\n"
            "Environment:\n"
            "  GOOGLE_CLOUD_PROJECT    GCP project for Vertex AI (default: johnb-2025)\n"
            "  CONVO_TRAINER_MODEL     Gemini model (default: gemini-3.1-pro-preview)\n"
            "  CONVO_TRAINER_TIMEOUT   Per-turn timeout seconds (default: 120)\n"
            "  HUMAN_PROVIDER          Provider for h-uman agent\n"
            "  HUMAN_MODEL             Model for h-uman agent\n"
        ),
    )
    parser.add_argument("--turns", type=int, default=20, help="Number of conversation turns (default: 20)")
    parser.add_argument("--scenario", default="mixed", help="Scenario profile (default: mixed)")
    parser.add_argument("--human-bin", default=str(os.environ.get("HUMAN_BIN", DEFAULT_HUMAN)),
                        help="Path to human binary")
    parser.add_argument("--output", default="convo-training", help="Output directory (default: convo-training/)")
    parser.add_argument("--session", default="", help="Session ID (default: auto-generated)")
    parser.add_argument("--dpo-threshold", type=int, default=60,
                        help="Score below which DPO pairs are collected (default: 60)")
    parser.add_argument("--persona", default=None,
                        help="Persona name (sets HUMAN_PERSONA env var for h-uman)")
    parser.add_argument("--provider", default=None,
                        help="Override provider for h-uman (sets HUMAN_PROVIDER)")
    parser.add_argument("--model", default=None,
                        help="Override model for h-uman (sets HUMAN_MODEL)")
    parser.add_argument("--dry-run", action="store_true", help="Simulate without calling LLMs")
    parser.add_argument("--verbose", action="store_true", help="Show full conversation as it happens")
    args = parser.parse_args()

    report = run_conversation(args)
    avg = report["aggregate"]["overall_mean"]
    sys.exit(0 if avg >= 50 else 1)


if __name__ == "__main__":
    main()
