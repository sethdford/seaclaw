#!/usr/bin/env bash
# eval-llm-judge.sh — LLM-as-judge conversation quality eval.
# Uses h-uman CLI for conversations, Gemini API directly for evaluation.
#
# Usage: bash scripts/eval-llm-judge.sh [--verbose]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
cd "$ROOT"

BUILD="${BUILD:-build}"
BINARY="$BUILD/human"
VERBOSE="${1:-}"
PROJECT="johnb-2025"
MODEL="gemini-3.1-flash-lite-preview"
ENDPOINT="https://aiplatform.googleapis.com/v1/projects/$PROJECT/locations/global/publishers/google/models/$MODEL:generateContent"

if [ ! -x "$BINARY" ]; then
    echo "Error: $BINARY not found." >&2; exit 1
fi

TOKEN=$(gcloud auth print-access-token 2>/dev/null || echo "")
if [ -z "$TOKEN" ]; then
    echo "Error: gcloud auth not available." >&2; exit 1
fi

pass=0; fail=0; total=0; total_score=0

# ── Send message to h-uman ────────────────────────────────────────────
send_message() {
    printf "%s\nexit\n" "$1" | "$BINARY" agent 2>/dev/null | \
        sed 's/\x1b\[[0-9;]*m//g' | \
        grep -v "^Human v\|^Provider:\|^Type your\|^> $\|^Goodbye\|^\[human\]" | \
        sed 's/^> //' | grep -v "^$" || echo "(no response)"
}

# ── Call Gemini directly for evaluation ───────────────────────────────
call_gemini() {
    local prompt="$1"
    local escaped
    escaped=$(printf '%s' "$prompt" | python3 -c "import sys,json; print(json.dumps(sys.stdin.read()))")

    local response
    response=$(curl -s -X POST "$ENDPOINT" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":$escaped}]}],\"generationConfig\":{\"temperature\":0.1}}" 2>/dev/null)

    echo "$response" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    print(d['candidates'][0]['content']['parts'][0]['text'])
except: print('ERROR: could not parse response')
" 2>/dev/null
}

# ── Run one scenario ──────────────────────────────────────────────────
run_scenario() {
    local name="$1"
    local msg="$2"
    total=$((total + 1))
    printf "  [%02d] %-30s " "$total" "$name"

    local response
    response=$(send_message "$msg")
    local rlen=${#response}

    # Ask Gemini to judge
    local judge_prompt="You are evaluating a persona-based AI chatbot that should sound like Seth Ford: a casual, lowercase-texting tech professional and single dad from Utah. The chatbot should NOT sound like a generic AI.

USER MESSAGE: $msg
CHATBOT RESPONSE: $response

Score 0-10 on each axis. Be harsh — a generic AI response scores 0-3 even if helpful.

1. PERSONA (0-10): Does it sound like a real person texting? Lowercase, casual, personality?
2. ENGAGEMENT (0-10): Does it ask follow-ups, share personal thoughts, show genuine interest?
3. NATURALNESS (0-10): Would this pass as a real human text message? No AI-speak?
4. SUBSTANCE (0-10): Real content, not just filler words?
5. WARMTH (0-10): Friendly, relatable, emotionally present?

Reply EXACTLY in this format (numbers only after colons):
PERSONA: <0-10>
ENGAGEMENT: <0-10>
NATURALNESS: <0-10>
SUBSTANCE: <0-10>
WARMTH: <0-10>
TOTAL: <sum>
VERDICT: PASS or FAIL (PASS if TOTAL >= 30)"

    local judge_result
    judge_result=$(call_gemini "$judge_prompt")

    local score
    score=$(echo "$judge_result" | grep "^TOTAL:" | grep -oE '[0-9]+' | head -1)
    local verdict
    verdict=$(echo "$judge_result" | grep "^VERDICT:" | awk '{print $2}' | head -1)

    [ -z "$score" ] && score=0
    [ -z "$verdict" ] && verdict="FAIL"

    local pct=$((score * 2))  # 50 max → 100%
    total_score=$((total_score + pct))

    if [ "$verdict" = "PASS" ]; then
        printf "\033[32mPASS\033[0m %3d%% (%d chars)\n" "$pct" "$rlen"
        pass=$((pass + 1))
    else
        printf "\033[31mFAIL\033[0m %3d%% (%d chars)\n" "$pct" "$rlen"
        fail=$((fail + 1))
    fi

    if [ "$VERBOSE" = "--verbose" ]; then
        printf "      \033[2mUser:\033[0m %s\n" "$msg"
        printf "      \033[2mResponse:\033[0m %.200s\n" "$response"
        echo "$judge_result" | grep -E "^[A-Z]+:" | sed 's/^/      /'
        echo ""
    fi
}

# ── Main ──────────────────────────────────────────────────────────────
echo ""
echo "============================================"
echo "  h-uman LLM-as-Judge Eval"
echo "============================================"
echo "  Chatbot: $BINARY ($(./build/human --version 2>&1))"
echo "  Judge: Gemini ($MODEL via Vertex AI)"
echo "  Scoring: 5 axes x 10 points = 50 max"
echo ""

run_scenario "Casual greeting" \
    "Hey! How is it going today?"

run_scenario "Personal/emotional" \
    "What is your favorite thing about being a dad?"

run_scenario "Technical help" \
    "Can you help me debug a Python script that is throwing an IndexError?"

run_scenario "Deep/philosophical" \
    "Do you ever think about what makes us who we are?"

run_scenario "Humor/playful" \
    "Dude I just spent 3 hours debugging and the fix was a missing comma."

run_scenario "Family context" \
    "Edison just texted me something hilarious. Kids are so funny."

run_scenario "Work stress" \
    "I am so done with these deployment failures. Been at it since 6am."

run_scenario "Quick technical" \
    "What is the best way to handle async errors in Node.js?"

run_scenario "Empathy/support" \
    "I am having a really rough week. My dog is sick and work is overwhelming."

run_scenario "Enthusiastic news" \
    "I just got promoted at work! Still in shock honestly."

# ── Summary ───────────────────────────────────────────────────────────
echo ""
echo "============================================"
avg=$((total_score / total))
if [ "$fail" -eq 0 ]; then
    printf "  \033[32mAll %d scenarios passed.\033[0m\n" "$pass"
else
    printf "  \033[33m%d/%d passed, %d failed.\033[0m\n" "$pass" "$total" "$fail"
fi
printf "  Average quality: %d%% (LLM judge)\n" "$avg"
echo ""
if [ "$avg" -ge 70 ]; then
    printf "  \033[32mOVERALL: PASS (>= 70%%)\033[0m\n"
elif [ "$avg" -ge 50 ]; then
    printf "  \033[33mOVERALL: MARGINAL (50-69%%)\033[0m\n"
else
    printf "  \033[31mOVERALL: FAIL (< 50%%)\033[0m\n"
fi
echo "============================================"
echo ""
exit "$fail"
