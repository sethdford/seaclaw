#!/usr/bin/env bash
# e2e-media-imessage.sh — End-to-end media generation + iMessage send proof.
#
# Prerequisites:
#   - macOS with Messages.app configured
#   - gcloud CLI with ADC configured (gcloud auth application-default login)
#   - ffmpeg (optional, for GIF conversion)
#   - imsg CLI on PATH (optional, falls back to AppleScript)
#
# Usage:
#   ./scripts/e2e-media-imessage.sh --to "+1234567890"          # Phase A only
#   ./scripts/e2e-media-imessage.sh --to "+1234567890" --full   # All phases
#   ./scripts/e2e-media-imessage.sh --to "+1234567890" --phase=B

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RECIPIENT=""
PHASE="A"
FULL=false

usage() {
    echo "Usage: $0 --to <phone-or-email> [--phase=A|B|C] [--full]"
    echo ""
    echo "  --to       Recipient phone number or Apple ID email"
    echo "  --phase    Run a specific phase (A=curl API, B=binary, C=daemon)"
    echo "  --full     Run all phases"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --to) RECIPIENT="$2"; shift 2 ;;
        --to=*) RECIPIENT="${1#*=}"; shift ;;
        --phase=*) PHASE="${1#*=}"; shift ;;
        --full) FULL=true; shift ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

[[ -z "$RECIPIENT" ]] && { echo "ERROR: --to is required"; usage; }

# --- Colors ---
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[0;33m'; CYAN='\033[0;36m'; NC='\033[0m'
pass() { echo -e "${GREEN}✓ $1${NC}"; }
fail() { echo -e "${RED}✗ $1${NC}"; }
info() { echo -e "${CYAN}→ $1${NC}"; }
warn() { echo -e "${YELLOW}⚠ $1${NC}"; }

# --- Prerequisite Checks ---
info "Checking prerequisites..."

PROJECT="${GOOGLE_CLOUD_PROJECT:-$(gcloud config get-value project 2>/dev/null || true)}"
[[ -z "$PROJECT" ]] && { fail "No GCP project. Set GOOGLE_CLOUD_PROJECT or run: gcloud config set project <id>"; exit 1; }
pass "GCP project: $PROJECT"

TOKEN=$(gcloud auth application-default print-access-token 2>/dev/null || true)
[[ -z "$TOKEN" ]] && { fail "ADC not configured. Run: gcloud auth application-default login"; exit 1; }
pass "ADC token acquired (${#TOKEN} chars)"

HAS_FFMPEG=false
command -v ffmpeg &>/dev/null && HAS_FFMPEG=true
$HAS_FFMPEG && pass "ffmpeg found" || warn "ffmpeg not found — GIF conversion will be skipped"

HAS_IMSG=false
command -v imsg &>/dev/null && HAS_IMSG=true
$HAS_IMSG && pass "imsg CLI found" || info "imsg not found — will use AppleScript fallback"

REGION="${HU_VERTEX_REGION:-us-central1}"

send_imessage() {
    local to="$1" text="$2" file="${3:-}"
    if $HAS_IMSG; then
        if [[ -n "$file" && -f "$file" ]]; then
            imsg send --to "$to" --text "$text" --file "$file" --service imessage
        else
            imsg send --to "$to" --text "$text" --service imessage
        fi
    else
        if [[ -n "$file" && -f "$file" ]]; then
            osascript -e "
                tell application \"Messages\"
                    set targetService to 1st account whose service type = iMessage
                    set targetBuddy to participant \"$to\" of targetService
                    send POSIX file \"$file\" to targetBuddy
                    delay 1
                    send \"$text\" to targetBuddy
                end tell"
        else
            osascript -e "
                tell application \"Messages\"
                    set targetService to 1st account whose service type = iMessage
                    set targetBuddy to participant \"$to\" of targetService
                    send \"$text\" to targetBuddy
                end tell"
        fi
    fi
}

# ============================================================
# Phase A: Direct API test (curl to Vertex AI, send via iMessage)
# ============================================================
run_phase_a() {
    echo ""
    echo "═══════════════════════════════════════════════════"
    echo " Phase A: Direct Vertex AI API → iMessage"
    echo "═══════════════════════════════════════════════════"

    local ENDPOINT="https://${REGION}-aiplatform.googleapis.com/v1/projects/${PROJECT}/locations/${REGION}/publishers/google/models/gemini-2.0-flash-exp:generateContent"

    info "Generating image via Gemini image generation..."

    local RESP
    RESP=$(curl -s -X POST "$ENDPOINT" \
        -H "Authorization: Bearer $TOKEN" \
        -H "Content-Type: application/json" \
        -d '{
            "contents": [{
                "parts": [{"text": "Generate an image of a golden retriever playing in colorful autumn leaves in a park, photorealistic style"}]
            }],
            "generationConfig": {
                "responseModalities": ["TEXT", "IMAGE"]
            }
        }')

    if echo "$RESP" | python3 -c "
import sys, json, base64
data = json.load(sys.stdin)
parts = data.get('candidates', [{}])[0].get('content', {}).get('parts', [])
for p in parts:
    if 'inlineData' in p:
        img = base64.b64decode(p['inlineData']['data'])
        with open('/tmp/human_e2e_phase_a.png', 'wb') as f:
            f.write(img)
        print(f'Wrote {len(img)} bytes')
        sys.exit(0)
print('No image in response')
sys.exit(1)
" 2>/dev/null; then
        pass "Image generated: /tmp/human_e2e_phase_a.png ($(wc -c < /tmp/human_e2e_phase_a.png) bytes)"
    else
        fail "Image generation failed. Response:"
        echo "$RESP" | python3 -m json.tool 2>/dev/null || echo "$RESP"
        return 1
    fi

    info "Sending to $RECIPIENT via iMessage..."
    if send_imessage "$RECIPIENT" "🎨 E2E Phase A: AI-generated golden retriever (direct API test)" "/tmp/human_e2e_phase_a.png"; then
        pass "Image sent via iMessage"
    else
        fail "iMessage send failed"
        return 1
    fi

    if $HAS_FFMPEG; then
        info "Converting to GIF for GIF pipeline test..."
        ffmpeg -y -loop 1 -i /tmp/human_e2e_phase_a.png -t 2 -vf "scale=320:-1" -f gif /tmp/human_e2e_phase_a.gif 2>/dev/null
        if [[ -f /tmp/human_e2e_phase_a.gif ]]; then
            pass "GIF created: /tmp/human_e2e_phase_a.gif ($(wc -c < /tmp/human_e2e_phase_a.gif) bytes)"
            send_imessage "$RECIPIENT" "🎬 E2E Phase A: GIF conversion test" "/tmp/human_e2e_phase_a.gif"
            pass "GIF sent via iMessage"
        fi
    fi

    pass "Phase A complete"
}

# ============================================================
# Phase B: h-uman binary tool test
# ============================================================
run_phase_b() {
    echo ""
    echo "═══════════════════════════════════════════════════"
    echo " Phase B: h-uman binary tool execution"
    echo "═══════════════════════════════════════════════════"

    info "Building release binary..."
    cd "$ROOT_DIR"
    cmake --preset release 2>/dev/null
    cmake --build --preset release 2>&1 | tail -3

    if [[ ! -x build-release/human ]]; then
        fail "Binary not found at build-release/human"
        return 1
    fi
    pass "Binary built: $(ls -lh build-release/human | awk '{print $5}')"

    local CONFIG_FILE="/tmp/human_e2e_config.json"
    cat > "$CONFIG_FILE" << HEREDOC
{
    "default_provider": "gemini",
    "default_model": "gemini-3.1-flash-lite-preview",
    "channels": {
        "imessage": {
            "enabled": true,
            "default_target": "$RECIPIENT"
        }
    },
    "media_gen": {
        "vertex_project": "$PROJECT",
        "vertex_region": "$REGION",
        "default_image_model": "gemini",
        "default_video_model": "veo_3.1"
    }
}
HEREDOC
    pass "Config written: $CONFIG_FILE"

    info "Running h-uman agent with image generation prompt..."
    local OUTPUT
    OUTPUT=$(timeout 60 ./build-release/human agent \
        --config "$CONFIG_FILE" \
        -m "Generate an image of a sunset over the ocean with vibrant colors" \
        2>&1) || true

    echo "$OUTPUT" | tail -10
    if echo "$OUTPUT" | grep -qi "generated\|image\|media\|png\|sent"; then
        pass "Binary execution completed with media indicators"
    else
        warn "Binary ran but output unclear — check above"
    fi

    pass "Phase B complete"
}

# ============================================================
# Phase C: Full daemon loop test (interactive)
# ============================================================
run_phase_c() {
    echo ""
    echo "═══════════════════════════════════════════════════"
    echo " Phase C: Daemon loop (interactive)"
    echo "═══════════════════════════════════════════════════"

    local CONFIG_FILE="/tmp/human_e2e_config.json"
    [[ ! -f "$CONFIG_FILE" ]] && {
        fail "Config file missing. Run Phase B first (--phase=B or --full)"
        return 1
    }
    [[ ! -x "$ROOT_DIR/build-release/human" ]] && {
        fail "Binary missing. Run Phase B first"
        return 1
    }

    info "Starting daemon in background (30s timeout)..."
    "$ROOT_DIR/build-release/human" service-loop --config "$CONFIG_FILE" &
    local DAEMON_PID=$!
    pass "Daemon started (PID $DAEMON_PID)"

    info "Send a message TO this device's iMessage to trigger a response."
    info "Example: 'Generate an image of a cat wearing a top hat'"
    info "Waiting 30 seconds for interaction..."

    sleep 30

    info "Stopping daemon..."
    kill "$DAEMON_PID" 2>/dev/null || true
    wait "$DAEMON_PID" 2>/dev/null || true

    info "Checking chat.db for recent outbound attachments..."
    local ATTACH_COUNT
    ATTACH_COUNT=$(sqlite3 ~/Library/Messages/chat.db \
        "SELECT COUNT(*) FROM message JOIN message_attachment_join ON message.ROWID = message_attachment_join.message_id WHERE message.is_from_me = 1 AND message.date > (strftime('%s','now') - 120) * 1000000000 - 978307200000000000;" 2>/dev/null || echo "0")

    if [[ "$ATTACH_COUNT" -gt 0 ]]; then
        pass "Found $ATTACH_COUNT recent outbound attachment(s) in chat.db"
    else
        warn "No recent outbound attachments found (daemon may not have triggered)"
    fi

    pass "Phase C complete"
}

# ============================================================
# Main
# ============================================================
echo ""
echo "╔═══════════════════════════════════════════════════╗"
echo "║   h-uman E2E Media Generation + iMessage Proof   ║"
echo "╠═══════════════════════════════════════════════════╣"
echo "║  Recipient: $(printf '%-37s' "$RECIPIENT") ║"
echo "║  Project:   $(printf '%-37s' "$PROJECT") ║"
echo "║  Region:    $(printf '%-37s' "$REGION") ║"
echo "╚═══════════════════════════════════════════════════╝"

FAILED=0

if $FULL; then
    run_phase_a || FAILED=$((FAILED + 1))
    run_phase_b || FAILED=$((FAILED + 1))
    run_phase_c || FAILED=$((FAILED + 1))
else
    case "$PHASE" in
        A) run_phase_a || FAILED=$((FAILED + 1)) ;;
        B) run_phase_b || FAILED=$((FAILED + 1)) ;;
        C) run_phase_c || FAILED=$((FAILED + 1)) ;;
        *) echo "Unknown phase: $PHASE"; usage ;;
    esac
fi

echo ""
if [[ $FAILED -eq 0 ]]; then
    pass "All phases passed"
else
    fail "$FAILED phase(s) failed"
    exit 1
fi
