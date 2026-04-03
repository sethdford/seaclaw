#!/usr/bin/env bash
# test-voice-e2e.sh — Send real voice messages to test the full Cartesia TTS pipeline.
#
# Prerequisites:
#   - CARTESIA_API_KEY environment variable set
#   - macOS with iMessage configured
#   - ffmpeg with libopus (brew install ffmpeg)
#
# Usage:
#   CARTESIA_API_KEY=sk-... ./scripts/test-voice-e2e.sh [voice_id] [recipient]
#
# Modes:
#   --all       Send all emotion test messages (default: single sympathetic)
#   --ssml      Test with SSML tags in transcript
#   --strip     Test with SSML stripped (punctuation fallback)
#   --opus      Force Opus-in-CAF (default when ffmpeg available)
#   --aac       Force AAC-in-CAF via afconvert
#
# Defaults:
#   voice_id = 9b7bffe4-37e8-4c16-b97a-1f5ff8d1f437
#   recipient = (required)

set -euo pipefail

VOICE_ID="${1:-9b7bffe4-37e8-4c16-b97a-1f5ff8d1f437}"
RECIPIENT="${2:-}"
MODE="${3:-}"

if [ -z "${CARTESIA_API_KEY:-}" ]; then
    if [ -f "$HOME/.human/.env" ]; then
        # shellcheck disable=SC1091
        set -a && source "$HOME/.human/.env" && set +a
    fi
fi
if [ -z "${CARTESIA_API_KEY:-}" ]; then
    echo "ERROR: CARTESIA_API_KEY not set"
    echo "Usage: CARTESIA_API_KEY=sk-... $0 [voice_id] recipient@email.com [--all|--ssml|--strip]"
    exit 1
fi

if [ -z "$RECIPIENT" ]; then
    echo "ERROR: recipient not specified"
    echo "Usage: CARTESIA_API_KEY=sk-... $0 [voice_id] recipient@email.com [--all|--ssml|--strip]"
    exit 1
fi

MODEL="sonic-3-2026-01-12"
TMPDIR_E2E=$(mktemp -d)
PASS=0
FAIL=0

synthesize_and_send() {
    local label="$1"
    local transcript="$2"
    local emotion="$3"
    local speed="$4"
    local volume="$5"

    echo ""
    echo "── $label ──"
    echo "  Emotion: $emotion | Speed: $speed | Volume: $volume"
    echo "  Text: ${transcript:0:80}..."

    local mp3_file="$TMPDIR_E2E/${label// /_}.mp3"
    local caf_file="$TMPDIR_E2E/${label// /_}.caf"

    local json_file="$TMPDIR_E2E/${label// /_}.json"
    python3 -c "
import json, sys
payload = {
    'model_id': sys.argv[1],
    'transcript': sys.argv[2],
    'voice': {'mode': 'id', 'id': sys.argv[3]},
    'output_format': {'container': 'mp3', 'sample_rate': 44100, 'bit_rate': 128000},
    'generation_config': {
        'speed': float(sys.argv[4]),
        'emotion': sys.argv[5],
        'volume': float(sys.argv[6]),
        'nonverbals': True
    }
}
with open(sys.argv[7], 'w') as f:
    json.dump(payload, f)
" "$MODEL" "$transcript" "$VOICE_ID" "$speed" "$emotion" "$volume" "$json_file"

    local http_code
    http_code=$(curl -s -w "%{http_code}" -o "$mp3_file" \
        -X POST "https://api.cartesia.ai/tts/bytes" \
        -H "X-API-Key: $CARTESIA_API_KEY" \
        -H "Cartesia-Version: 2026-03-01" \
        -H "Content-Type: application/json" \
        -d @"$json_file")

    if [ "$http_code" != "200" ]; then
        echo "  FAIL: Cartesia API returned HTTP $http_code"
        FAIL=$((FAIL + 1))
        return 1
    fi

    local mp3_size
    mp3_size=$(stat -f%z "$mp3_file" 2>/dev/null || stat -c%s "$mp3_file" 2>/dev/null)
    echo "  MP3: $mp3_size bytes"

    local send_file="$mp3_file"

    if [ "$MODE" = "--aac" ]; then
        if command -v afconvert &>/dev/null; then
            afconvert "$mp3_file" "$caf_file" -d aac -f caff -b 128000
            send_file="$caf_file"
            echo "  Encoded: AAC-in-CAF (afconvert)"
        fi
    else
        if command -v ffmpeg &>/dev/null; then
            ffmpeg -y -loglevel error -i "$mp3_file" \
                -c:a libopus -ar 24000 -ac 1 -b:a 24000 \
                -f caf "$caf_file" 2>/dev/null
            if [ -f "$caf_file" ]; then
                local caf_size
                caf_size=$(stat -f%z "$caf_file" 2>/dev/null || stat -c%s "$caf_file" 2>/dev/null)
                send_file="$caf_file"
                echo "  Encoded: Opus-in-CAF ($caf_size bytes) — native voice bubble"
            fi
        elif command -v afconvert &>/dev/null; then
            afconvert "$mp3_file" "$caf_file" -d aac -f caff -b 128000
            send_file="$caf_file"
            echo "  Encoded: AAC-in-CAF (fallback)"
        fi
    fi

    if command -v imsg &>/dev/null; then
        imsg send --to "$RECIPIENT" --file "$send_file" --service imessage
        echo "  Sent via imsg CLI"
    else
        osascript -e "
            tell application \"Messages\"
                set targetBuddy to buddy \"$RECIPIENT\" of (service 1 whose service type is iMessage)
                send POSIX file \"$send_file\" to targetBuddy
            end tell
        "
        echo "  Sent via AppleScript"
    fi

    PASS=$((PASS + 1))
    echo "  OK"
}

echo "=== Voice Message E2E Test Suite ==="
echo "Voice ID:  $VOICE_ID"
echo "Recipient: $RECIPIENT"
echo "Model:     $MODEL"
echo "Mode:      ${MODE:-default (Opus-in-CAF + SSML)}"

# ── Test 1: Sympathetic (warm, slow, quiet) ──
synthesize_and_send "sympathetic" \
    "<emotion value=\"sympathetic\"/>Hey, I just wanted to check in on you. <break time=\"300ms\"/>I know things have been tough lately, <break time=\"200ms\"/>but I'm really proud of how you've been handling everything. <speed ratio=\"0.90\"/>You're stronger than you think." \
    "sympathetic" "0.92" "0.90"

if [ "$MODE" = "--all" ]; then
    sleep 2

    # ── Test 2: Excited (fast, loud) ──
    synthesize_and_send "excited" \
        "<emotion value=\"excited\"/>Oh my god, you're not going to believe this! <break time=\"150ms\"/><speed ratio=\"1.10\"/>I just found out the most amazing news and I had to tell you right away!" \
        "excited" "1.05" "1.15"

    sleep 2

    # ── Test 3: Contemplative (slow, medium volume, thoughtful) ──
    synthesize_and_send "contemplative" \
        "<emotion value=\"contemplative\"/><speed ratio=\"0.88\"/>You know, <break time=\"400ms\"/>I've been thinking a lot about what you said. <break time=\"300ms\"/>And I think there's something really true about it. <break time=\"350ms\"/>Like, <break time=\"200ms\"/>maybe we don't always have to have all the answers right away." \
        "contemplative" "0.88" "0.95"

    sleep 2

    # ── Test 4: Playful (natural speed, bright) ──
    synthesize_and_send "playful" \
        "<emotion value=\"playful\"/>Okay but hear me out, <break time=\"150ms\"/>what if we just, <break time=\"100ms\"/>I don't know, <break time=\"200ms\"/>took the day off and went on an adventure? [laughter] <break time=\"300ms\"/>I'm totally serious!" \
        "playful" "1.00" "1.05"

    sleep 2

    # ── Test 5: Emotional arc (multiple emotions in one message) ──
    synthesize_and_send "emotional-arc" \
        "<emotion value=\"sad\"/><speed ratio=\"0.85\"/><volume ratio=\"0.85\"/>I miss you. <break time=\"500ms\"/><emotion value=\"nostalgic\"/><volume ratio=\"0.90\"/>Remember when we used to stay up all night just talking about nothing? <break time=\"400ms\"/><emotion value=\"hopeful\"/><speed ratio=\"0.95\"/><volume ratio=\"1.00\"/>I really hope we can do that again soon. <break time=\"300ms\"/><emotion value=\"warm\"/><volume ratio=\"1.05\"/>You mean the world to me." \
        "nostalgic" "0.90" "0.95"

    sleep 2

    # ── Test 6: Strip-SSML mode (punctuation only, no tags) ──
    synthesize_and_send "strip-ssml-fallback" \
        "Hey, I just wanted to check in on you. I know things have been tough lately, but I'm really proud of how you've been handling everything... You're stronger than you think." \
        "sympathetic" "0.92" "0.90"
fi

echo ""
echo "=== Results ==="
echo "Passed: $PASS"
echo "Failed: $FAIL"
rm -rf "$TMPDIR_E2E"

if [ "$FAIL" -gt 0 ]; then
    echo "SOME TESTS FAILED"
    exit 1
fi
echo "ALL TESTS PASSED"
