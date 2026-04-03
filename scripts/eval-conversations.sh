#!/usr/bin/env bash
# eval-conversations.sh — Synthetic conversation quality eval.
# Tests h-uman CLI with 10 scenarios, evaluates responses heuristically
# + optionally via Gemini API for deep scoring.
#
# Usage: bash scripts/eval-conversations.sh [--verbose]
#        make eval-convo
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
cd "$ROOT"

BUILD="${BUILD:-build}"
BINARY="$BUILD/human"
VERBOSE="${1:-}"

if [ ! -x "$BINARY" ]; then
    echo "Error: $BINARY not found. Run: cmake --build build" >&2
    exit 1
fi

pass=0
fail=0
total=0
total_score=0

# ── Helper: send message, capture response ────────────────────────────
send_message() {
    local msg="$1"
    printf "%s\nexit\n" "$msg" | "$BINARY" agent 2>/dev/null | \
        sed 's/\x1b\[[0-9;]*m//g' | \
        grep -v "^Human v" | \
        grep -v "^Provider:" | \
        grep -v "^Type your" | \
        grep -v "^> $" | \
        grep -v "^Goodbye" | \
        sed 's/^\[human\].*//' | \
        sed 's/^> //' | \
        grep -v "^$" || echo "(no response)"
}

# ── Heuristic evaluator ───────────────────────────────────────────────
# Scores 0-100 based on observable quality signals.
evaluate_heuristic() {
    local scenario="$1"
    local user_msg="$2"
    local response="$3"
    local score=0
    local notes=""

    local rlen=${#response}
    local ulen=${#user_msg}

    # 1. Length check (0-20 points)
    if [ "$rlen" -ge 200 ]; then
        score=$((score + 20))
    elif [ "$rlen" -ge 100 ]; then
        score=$((score + 15))
    elif [ "$rlen" -ge 50 ]; then
        score=$((score + 10))
    elif [ "$rlen" -ge 20 ]; then
        score=$((score + 5))
    else
        notes="${notes}too_short "
    fi

    # 2. Persona markers — lowercase, casual style (0-20 points)
    if echo "$response" | grep -qE "^[a-z]"; then
        score=$((score + 10))  # starts lowercase = persona
    else
        notes="${notes}uppercase_start "
    fi
    if echo "$response" | grep -qiE "lol|haha|lmao|omg|tbh|ngl|ugh|hru|yo|dude|bruh|vibe"; then
        score=$((score + 10))  # casual slang markers
    elif echo "$response" | grep -qiE "gonna|gotta|wanna|kinda|sorta|ain't|y'all"; then
        score=$((score + 7))
    fi

    # 3. Engagement — asks follow-up question (0-20 points)
    if echo "$response" | grep -q "?"; then
        score=$((score + 15))
        if echo "$response" | grep -cq "?" | grep -qE "[2-9]"; then
            score=$((score + 5))  # multiple questions = very engaged
        fi
    else
        notes="${notes}no_question "
    fi

    # 4. Naturalness — no AI markers (0-20 points)
    local ai_score=20
    if echo "$response" | grep -qiE "as an ai|i cannot|i do not have|i am an ai|language model"; then
        ai_score=0
        notes="${notes}ai_markers "
    elif echo "$response" | grep -qiE "certainly|furthermore|additionally|delighted to|happy to help"; then
        ai_score=10
        notes="${notes}formal_markers "
    fi
    score=$((score + ai_score))

    # 5. Substance — not just filler (0-20 points)
    local word_count
    word_count=$(echo "$response" | wc -w | tr -d ' ')
    if [ "$word_count" -ge 30 ]; then
        score=$((score + 15))
    elif [ "$word_count" -ge 15 ]; then
        score=$((score + 10))
    elif [ "$word_count" -ge 5 ]; then
        score=$((score + 5))
    fi
    # Bonus: shares personal detail or asks about user
    if echo "$response" | grep -qiE "my |i was|i just|i have|i feel|kids|work|coffee"; then
        score=$((score + 5))
    fi

    # Cap at 100
    if [ "$score" -gt 100 ]; then
        score=100
    fi

    # Return score and notes
    echo "$score|$notes"
}

# ── Run one test scenario ─────────────────────────────────────────────
run_scenario() {
    local name="$1"
    local msg="$2"

    total=$((total + 1))
    printf "  [%02d] %-35s " "$total" "$name"

    local response
    response=$(send_message "$msg")

    local eval_result
    eval_result=$(evaluate_heuristic "$name" "$msg" "$response")
    local score
    score=$(echo "$eval_result" | cut -d'|' -f1)
    local notes
    notes=$(echo "$eval_result" | cut -d'|' -f2)

    total_score=$((total_score + score))

    if [ "$score" -ge 60 ]; then
        printf "\033[32mPASS\033[0m %3d%%  " "$score"
        pass=$((pass + 1))
    else
        printf "\033[31mFAIL\033[0m %3d%%  " "$score"
        fail=$((fail + 1))
    fi

    # Show response length
    local rlen=${#response}
    printf "(%d chars)\n" "$rlen"

    if [ "$VERBOSE" = "--verbose" ]; then
        printf "      \033[2mUser:\033[0m %s\n" "$msg"
        printf "      \033[2mResponse:\033[0m %s\n" "$(echo "$response" | head -3)"
        if [ -n "$notes" ]; then
            printf "      \033[33mNotes:\033[0m %s\n" "$notes"
        fi
        echo ""
    fi
}

# ── Main ──────────────────────────────────────────────────────────────

echo ""
echo "============================================"
echo "  h-uman Conversation Quality Eval"
echo "============================================"
echo "  Binary: $BINARY"
echo "  Scoring: heuristic (length + persona + engagement + naturalness + substance)"
echo ""

run_scenario "Casual greeting" \
    "Hey! How is it going today?"

run_scenario "Personal/emotional" \
    "What is your favorite thing about being a dad?"

run_scenario "Technical help" \
    "Can you help me debug a Python script that is throwing an IndexError?"

run_scenario "Deep/philosophical" \
    "Do you ever think about what makes us who we are? Like how much is nature vs nurture?"

run_scenario "Humor/playful" \
    "Dude I just spent 3 hours debugging and the fix was a missing comma. Kill me."

run_scenario "Family context (Edison)" \
    "Edison just texted me something hilarious. Kids are so funny at that age."

run_scenario "Work stress/venting" \
    "I am so done with these deployment failures. Nothing works and I have been at it since 6am."

run_scenario "Quick factual" \
    "What is the best way to handle async errors in Node.js?"

run_scenario "Empathy/support" \
    "I am having a really rough week. My dog is sick and work is overwhelming."

run_scenario "Enthusiastic news" \
    "I just got promoted at work! Still cannot believe it actually happened!"

# ── Summary ───────────────────────────────────────────────────────────
echo ""
echo "============================================"
avg_score=$((total_score / total))
if [ "$fail" -eq 0 ]; then
    printf "  \033[32mAll %d scenarios passed.\033[0m\n" "$pass"
else
    printf "  \033[33m%d/%d passed, %d failed.\033[0m\n" "$pass" "$total" "$fail"
fi
printf "  Average quality score: %d%%\n" "$avg_score"
echo ""

if [ "$avg_score" -ge 70 ]; then
    printf "  \033[32mOVERALL: PASS (>= 70%% quality threshold)\033[0m\n"
elif [ "$avg_score" -ge 50 ]; then
    printf "  \033[33mOVERALL: MARGINAL (50-69%% quality)\033[0m\n"
else
    printf "  \033[31mOVERALL: FAIL (< 50%% quality)\033[0m\n"
fi

echo ""
echo "  Scoring rubric (each 0-20, total 0-100):"
echo "    - Length: >= 200 chars = 20, >= 100 = 15, >= 50 = 10"
echo "    - Persona: lowercase start + casual slang markers"
echo "    - Engagement: asks follow-up questions"
echo "    - Naturalness: no AI markers (as an AI, certainly, etc.)"
echo "    - Substance: word count + personal details"
echo "============================================"
echo ""

exit "$fail"
