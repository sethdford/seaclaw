#!/usr/bin/env bash
# test-voice-e2e.sh — Send a real voice message to Mindy via iMessage.
#
# Prerequisites:
#   - CARTESIA_API_KEY environment variable set
#   - macOS with iMessage configured
#   - Voice ID configured in persona or passed as argument
#
# Usage:
#   CARTESIA_API_KEY=sk-... ./scripts/test-voice-e2e.sh [voice_id] [recipient]
#
# Defaults:
#   voice_id = 9b7bffe4-37e8-4c16-b97a-1f5ff8d1f437
#   recipient = mindy (looks up in contacts / uses as iMessage handle)

set -euo pipefail

VOICE_ID="${1:-9b7bffe4-37e8-4c16-b97a-1f5ff8d1f437}"
RECIPIENT="${2:-}"

if [ -z "${CARTESIA_API_KEY:-}" ]; then
    echo "ERROR: CARTESIA_API_KEY not set"
    echo "Usage: CARTESIA_API_KEY=sk-... $0 [voice_id] [recipient]"
    exit 1
fi

if [ -z "$RECIPIENT" ]; then
    echo "ERROR: recipient not specified"
    echo "Usage: CARTESIA_API_KEY=sk-... $0 [voice_id] recipient@email.com"
    exit 1
fi

TRANSCRIPT="Hey, I just wanted to check in on you. I know things have been tough lately, but I'm really proud of how you've been handling everything. You're stronger than you think."
EMOTION="sympathetic"
MODEL="sonic-3-2026-01-12"
SPEED=0.92
VOLUME=0.90

echo "=== Voice Message E2E Test ==="
echo "Voice ID:   $VOICE_ID"
echo "Recipient:  $RECIPIENT"
echo "Model:      $MODEL"
echo "Emotion:    $EMOTION"
echo "Speed:      $SPEED"
echo "Volume:     $VOLUME"
echo "Transcript: ${TRANSCRIPT:0:60}..."
echo ""

TMPDIR_E2E=$(mktemp -d)
MP3_FILE="$TMPDIR_E2E/voice_message.mp3"
CAF_FILE="$TMPDIR_E2E/voice_message.caf"

echo "[1/4] Synthesizing via Cartesia TTS..."
HTTP_CODE=$(curl -s -w "%{http_code}" -o "$MP3_FILE" \
    -X POST "https://api.cartesia.ai/tts/bytes" \
    -H "X-API-Key: $CARTESIA_API_KEY" \
    -H "Cartesia-Version: 2026-03-01" \
    -H "Content-Type: application/json" \
    -d "{
        \"model_id\": \"$MODEL\",
        \"transcript\": \"<emotion value=\\\"$EMOTION\\\"/>$TRANSCRIPT\",
        \"voice\": {\"mode\": \"id\", \"id\": \"$VOICE_ID\"},
        \"output_format\": {\"container\": \"mp3\", \"sample_rate\": 44100, \"bit_rate\": 128000},
        \"generation_config\": {\"speed\": $SPEED, \"emotion\": \"$EMOTION\", \"volume\": $VOLUME, \"nonverbals\": true}
    }")

if [ "$HTTP_CODE" != "200" ]; then
    echo "ERROR: Cartesia API returned HTTP $HTTP_CODE"
    cat "$MP3_FILE"
    rm -rf "$TMPDIR_E2E"
    exit 1
fi

MP3_SIZE=$(stat -f%z "$MP3_FILE" 2>/dev/null || stat -c%s "$MP3_FILE" 2>/dev/null)
echo "  MP3 synthesized: $MP3_SIZE bytes"

echo "[2/4] Converting MP3 -> CAF for iMessage..."
if command -v afconvert &>/dev/null; then
    afconvert "$MP3_FILE" "$CAF_FILE" -d LEI16 -f caff
    CAF_SIZE=$(stat -f%z "$CAF_FILE" 2>/dev/null || stat -c%s "$CAF_FILE" 2>/dev/null)
    echo "  CAF converted: $CAF_SIZE bytes"
    SEND_FILE="$CAF_FILE"
else
    echo "  afconvert not found, sending MP3 directly"
    SEND_FILE="$MP3_FILE"
fi

echo "[3/4] Sending via iMessage to $RECIPIENT..."
if command -v imsg &>/dev/null; then
    imsg send --to "$RECIPIENT" --file "$SEND_FILE" --service imessage
    echo "  Sent via imsg CLI"
else
    osascript -e "
        tell application \"Messages\"
            set targetBuddy to buddy \"$RECIPIENT\" of (service 1 whose service type is iMessage)
            send POSIX file \"$SEND_FILE\" to targetBuddy
        end tell
    "
    echo "  Sent via AppleScript"
fi

echo "[4/4] Cleanup..."
rm -rf "$TMPDIR_E2E"

echo ""
echo "=== SUCCESS ==="
echo "Voice message sent to $RECIPIENT"
echo "Transcript: $TRANSCRIPT"
echo "Emotion: $EMOTION | Speed: $SPEED | Volume: $VOLUME"
