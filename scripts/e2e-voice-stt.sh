#!/usr/bin/env bash
# e2e-voice-stt.sh — Cartesia TTS → STT round-trip (no microphone).
#
# Default: multi-turn "conversation" — several TTS→STT segments with N-of-M keyword
# checks, fuzzy normalization, and alternate spellings (e.g. hexadecimal).
#
# Usage:
#   CARTESIA_API_KEY=... CARTESIA_VOICE_ID=... bash scripts/e2e-voice-stt.sh
#
# Environment:
#   CARTESIA_API_KEY   (required) Cartesia API key
#   CARTESIA_VOICE_ID  (required) Voice UUID for TTS (see Cartesia dashboard).
#                      Falls back to FERNI_VOICE_ID if unset (common in local .env).
#   CARTESIA_MODEL     (optional) TTS model_id; default sonic-3-2026-01-12
#   CARTESIA_STT_MODEL (optional) STT model; default ink-whisper
#   E2E_VOICE_STT_MODE (optional) multi | single — default multi (3 turns)
#
# Model matrix (local / scripted sweeps; CI often uses one cell per job):
#   E2E_VOICE_STT_MATRIX_TTS  Space-separated TTS model_ids (overrides single CARTESIA_MODEL)
#   E2E_VOICE_STT_MATRIX_STT  Space-separated STT models (overrides single CARTESIA_STT_MODEL)
#
# Flake tuning (multi mode — N-of-M per turn):
#   E2E_VOICE_STT_MULTI_MIN        Default min matches for turns 1–3 (default 3)
#   E2E_VOICE_STT_MULTI_MIN_TURN1  Override turn 1 min (falls back to MULTI_MIN)
#   E2E_VOICE_STT_MULTI_MIN_TURN2  Override turn 2 min
#   E2E_VOICE_STT_MULTI_MIN_TURN3  Override turn 3 min
#   E2E_VOICE_STT_MULTI_SPECS_TURN1  Optional comma/~ CSV for turn 1 (advanced)
#   (same for _TURN2 / _TURN3)
#
# Gateway WebSocket (full voice.transcribe path):
#   E2E_VOICE_STT_GATEWAY=1       After direct API checks, start human gateway and call
#                                voice.transcribe via scripts/e2e-voice-stt-gateway.mjs
#   E2E_VOICE_STT_GATEWAY_PORT    Listen port (default 3009)
#   E2E_VOICE_STT_HUMAN_BIN       Path to human binary (auto-detects build/human, build-test, …)
#
# Single-clip mode (E2E_VOICE_STT_MODE=single):
#   E2E_VOICE_STT_TRANSCRIPT   Override synthesized text
#   E2E_VOICE_STT_KEYWORD_SPECS Comma-separated keyword specs (see below)
#   E2E_VOICE_STT_KEYWORDS     Space-separated words (each is one spec, no alternates)
#   E2E_VOICE_STT_KEYWORDS_MIN Pass if at least this many specs match (default: all)
#
# Keyword spec syntax (comma-separated list):
#   word                     — substring must appear (after normalization)
#   hex~hexadecimal          — either substring matches (tilde = alternate)
#   a~b~c                    — any alternate matches
#
# If repo-root .env exists, it is sourced (set -a). Do not commit secrets.
#
# Requirements: bash, curl, jq; Node >= 18 with global WebSocket (Node 22+) if gateway enabled.
#
# See also: .github/workflows/e2e-cartesia-voice-stt.yml, scripts/e2e-live.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

if [ -f "$REPO_ROOT/.env" ]; then
    set -a
    # shellcheck source=/dev/null
    source "$REPO_ROOT/.env"
    set +a
fi

for cmd in curl jq; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: $cmd is required."
        exit 1
    fi
done

if [ -z "${CARTESIA_API_KEY:-}" ]; then
    echo "Error: CARTESIA_API_KEY is not set."
    echo "Usage: CARTESIA_API_KEY=... CARTESIA_VOICE_ID=... bash scripts/e2e-voice-stt.sh"
    exit 1
fi

CARTESIA_VOICE_ID="${CARTESIA_VOICE_ID:-${FERNI_VOICE_ID:-}}"
if [ -z "$CARTESIA_VOICE_ID" ]; then
    echo "Error: CARTESIA_VOICE_ID (or FERNI_VOICE_ID) is not set (TTS requires a voice UUID)."
    exit 1
fi

CARTESIA_VERSION="${CARTESIA_VERSION:-2026-03-01}"
E2E_MODE="${E2E_VOICE_STT_MODE:-multi}"

if [ -n "${E2E_VOICE_STT_MATRIX_TTS:-}" ]; then
    read -ra TTS_MATRIX <<<"$E2E_VOICE_STT_MATRIX_TTS"
else
    TTS_MATRIX=("${CARTESIA_MODEL:-sonic-3-2026-01-12}")
fi

if [ -n "${E2E_VOICE_STT_MATRIX_STT:-}" ]; then
    read -ra STT_MATRIX <<<"$E2E_VOICE_STT_MATRIX_STT"
else
    STT_MATRIX=("${CARTESIA_STT_MODEL:-ink-whisper}")
fi

MULTI_MIN_DEFAULT="${E2E_VOICE_STT_MULTI_MIN:-3}"
MIN_T1="${E2E_VOICE_STT_MULTI_MIN_TURN1:-$MULTI_MIN_DEFAULT}"
MIN_T2="${E2E_VOICE_STT_MULTI_MIN_TURN2:-$MULTI_MIN_DEFAULT}"
MIN_T3="${E2E_VOICE_STT_MULTI_MIN_TURN3:-$MULTI_MIN_DEFAULT}"

WAV=$(mktemp "${TMPDIR:-/tmp}/e2e-voice-stt.XXXXXX.wav")
GW_PID=""
E2E_HOME_GATEWAY=""
cleanup_all() {
    rm -f "$WAV"
    if [ -n "$GW_PID" ] && kill -0 "$GW_PID" 2>/dev/null; then
        kill "$GW_PID" 2>/dev/null || true
        wait "$GW_PID" 2>/dev/null || true
    fi
    GW_PID=""
    if [ -n "${E2E_HOME_GATEWAY:-}" ]; then
        rm -rf "$E2E_HOME_GATEWAY"
        E2E_HOME_GATEWAY=""
    fi
}
trap cleanup_all EXIT

detect_human_bin() {
    if [ -n "${E2E_VOICE_STT_HUMAN_BIN:-}" ]; then
        if [ -x "${E2E_VOICE_STT_HUMAN_BIN}" ]; then
            printf '%s' "${E2E_VOICE_STT_HUMAN_BIN}"
            return 0
        fi
        echo "Error: E2E_VOICE_STT_HUMAN_BIN is not executable: ${E2E_VOICE_STT_HUMAN_BIN}" >&2
        return 1
    fi
    for c in "$REPO_ROOT/build/human" "$REPO_ROOT/build-test/human" "$REPO_ROOT/build-release/human"; do
        if [ -x "$c" ]; then
            printf '%s' "$c"
            return 0
        fi
    done
    echo "Error: no human binary found under build/, build-test/, or build-release/ (set E2E_VOICE_STT_HUMAN_BIN)." >&2
    return 1
}

if [ "${E2E_VOICE_STT_GATEWAY:-0}" = "1" ]; then
    if ! command -v node >/dev/null 2>&1; then
        echo "Error: E2E_VOICE_STT_GATEWAY=1 requires node in PATH."
        exit 1
    fi
    if ! _hb_check=$(detect_human_bin 2>&1); then
        echo "$_hb_check" >&2
        echo "Build with: cmake --preset dev && cmake --build build --target human"
        exit 1
    fi
    unset _hb_check
fi

# --- Fuzzy normalization (lowercase + common STT splits / homophone-adjacent forms)
normalize_for_match() {
    printf '%s' "$1" | tr '[:upper:]' '[:lower:]' | sed \
        -e 's/hex[[:space:]]\{1,\}decimal/hexadecimal/g' \
        -e 's/hex-decimal/hexadecimal/g'
}

# One keyword spec may be "hexadecimal~hex decimal~hex-decimal" (alternates after norm).
spec_matches_norm() {
    local norm=$1
    local spec=$2
    local IFS='~'
    local -a alts
    read -ra alts <<<"$spec"
    local a trimmed
    for a in "${alts[@]}"; do
        trimmed=$(printf '%s' "$a" | tr '[:upper:]' '[:lower:]')
        trimmed="${trimmed#"${trimmed%%[![:space:]]*}"}"
        trimmed="${trimmed%"${trimmed##*[![:space:]]}"}"
        [ -z "$trimmed" ] && continue
        trimmed=$(normalize_for_match "$trimmed")
        if [[ "$norm" == *"$trimmed"* ]]; then
            return 0
        fi
    done
    return 1
}

# specs_csv: comma-separated specs; min_req: need at least this many spec hits.
assert_specs_min() {
    local raw_text=$1
    local specs_csv=$2
    local min_req=$3
    local norm
    norm=$(normalize_for_match "$raw_text")

    local IFS=','
    local -a specs
    read -ra specs <<<"$specs_csv"

    local n_specs=${#specs[@]}
    if [ "$n_specs" -eq 0 ]; then
        echo "Error: no keyword specs to check."
        return 1
    fi

    local hits=0
    local s
    echo "  Keyword check: need $min_req of $n_specs spec(s)"
    for s in "${specs[@]}"; do
        s="${s#"${s%%[![:space:]]*}"}"
        s="${s%"${s##*[![:space:]]}"}"
        [ -z "$s" ] && continue
        if spec_matches_norm "$norm" "$s"; then
            hits=$((hits + 1))
            echo "    OK: \"$s\""
        else
            echo "    MISS: \"$s\""
        fi
    done

    if [ "$hits" -lt "$min_req" ]; then
        echo "Error: only $hits spec(s) matched; need >= $min_req."
        echo "  Normalized transcript (excerpt): ${norm:0:400}..."
        return 1
    fi
    return 0
}

cartesia_tts_wav() {
    local transcript=$1
    local outpath=$2
    local payload
    payload=$(
        jq -n \
            --arg mid "$TTS_MODEL" \
            --arg tr "$transcript" \
            --arg vid "$CARTESIA_VOICE_ID" \
            '{
          model_id: $mid,
          transcript: $tr,
          voice: {mode: "id", id: $vid},
          output_format: {container: "wav", encoding: "pcm_s16le", sample_rate: 44100},
          generation_config: {speed: 0.95, emotion: "content", volume: 1.0, nonverbals: false}
        }'
    )
    local code
    code=$(
        curl -sS -o "$outpath" -w "%{http_code}" \
            -X POST "https://api.cartesia.ai/tts/bytes" \
            -H "X-API-Key: ${CARTESIA_API_KEY}" \
            -H "Cartesia-Version: ${CARTESIA_VERSION}" \
            -H "Content-Type: application/json" \
            -d "$payload"
    ) || true
    if [ "$code" != "200" ]; then
        echo "Error: TTS failed (HTTP $code)."
        [ -s "$outpath" ] && head -c 512 "$outpath" | cat -v
        return 1
    fi
    if [ ! -s "$outpath" ]; then
        echo "Error: TTS returned empty body."
        return 1
    fi
    return 0
}

cartesia_stt_text() {
    local wavpath=$1
    local raw
    raw=$(
        curl -sS -w "\n%{http_code}" \
            -X POST "https://api.cartesia.ai/stt" \
            -H "X-API-Key: ${CARTESIA_API_KEY}" \
            -H "Cartesia-Version: ${CARTESIA_VERSION}" \
            -F "file=@${wavpath}" \
            -F "model=${STT_MODEL}"
    )
    local http body
    http=$(echo "$raw" | tail -n1)
    body=$(echo "$raw" | sed '$d')
    if [ "$http" != "200" ]; then
        echo "Error: STT failed (HTTP $http)."
        echo "$body" | head -c 1024 | cat -v
        return 1
    fi
    local text
    text=$(echo "$body" | jq -r '.text // empty')
    if [ -z "$text" ]; then
        echo "Error: STT JSON missing .text"
        echo "$body" | head -c 1024 | cat -v
        return 1
    fi
    printf '%s' "$text"
}

run_one_segment() {
    local label=$1
    local transcript=$2
    local specs_csv=$3
    local min_req=$4

    echo ""
    echo "[e2e-voice-stt] $label — TTS → STT ($TTS_MODEL / $STT_MODEL)"
    echo "  Prompt: ${transcript:0:120}$([ "${#transcript}" -gt 120 ] && echo "...")"

    cartesia_tts_wav "$transcript" "$WAV" || exit 1
    echo "  WAV: $(wc -c <"$WAV" | tr -d ' ') bytes"

    local text
    text=$(cartesia_stt_text "$WAV") || exit 1
    echo "  Transcript: $text"

    assert_specs_min "$text" "$specs_csv" "$min_req" || exit 1
}

run_multi() {
    local s1 s2 s3
    s1="${E2E_VOICE_STT_MULTI_SPECS_TURN1:-hello,joining,hear,clearly,thank~thanks}"
    s2="${E2E_VOICE_STT_MULTI_SPECS_TURN2:-second,seattle,weather,turn,mention~mentioned}"
    s3="${E2E_VOICE_STT_MULTI_SPECS_TURN3:-final,hexadecimal~hex decimal~hex-decimal,goodbye,confirm}"

    echo "[e2e-voice-stt] Mode: multi (3 turns, N-of-M per turn; mins $MIN_T1 / $MIN_T2 / $MIN_T3)"
    run_one_segment "Turn 1 / 3" \
        "Hello, thank you for joining this call. I hope you can hear me clearly on your end." \
        "$s1" \
        "$MIN_T1"

    run_one_segment "Turn 2 / 3" \
        "This is the second turn. We mention Seattle and the weather before we continue." \
        "$s2" \
        "$MIN_T2"

    run_one_segment "Turn 3 / 3" \
        "Final turn: please confirm the hexadecimal check passed, then we say goodbye." \
        "$s3" \
        "$MIN_T3"

    echo ""
    echo "[e2e-voice-stt] Pass — multi-turn TTS→STT (all segments met N-of-M)."
}

run_single() {
    echo "[e2e-voice-stt] Mode: single"
    local transcript specs_csv min_req n_specs

    if [ -n "${E2E_VOICE_STT_TRANSCRIPT:-}" ]; then
        transcript=$E2E_VOICE_STT_TRANSCRIPT
    else
        transcript='Hello, this is the opening line of our longer speech test. In the middle we mention Seattle and the word hexadecimal so the transcript stays distinctive. Finally we thank you for listening and confirm the automated pipeline finished successfully.'
    fi

    if [ -n "${E2E_VOICE_STT_KEYWORD_SPECS:-}" ]; then
        specs_csv=$E2E_VOICE_STT_KEYWORD_SPECS
    elif [ -n "${E2E_VOICE_STT_KEYWORDS:-}" ]; then
        read -ra _kw <<<"$E2E_VOICE_STT_KEYWORDS"
        local IFS=,
        specs_csv="${_kw[*]}"
        unset IFS
    else
        specs_csv="hello,seattle,hexadecimal~hex decimal~hex-decimal,automated,successfully"
    fi

    IFS=',' read -ra _tmp <<<"$specs_csv"
    n_specs=${#_tmp[@]}
    if [ -n "${E2E_VOICE_STT_KEYWORDS_MIN:-}" ]; then
        min_req=$E2E_VOICE_STT_KEYWORDS_MIN
    else
        min_req=$n_specs
    fi

    if ! [[ "$min_req" =~ ^[0-9]+$ ]] || [ "$min_req" -lt 1 ]; then
        echo "Error: E2E_VOICE_STT_KEYWORDS_MIN must be a positive integer."
        exit 1
    fi
    if [ "$min_req" -gt "$n_specs" ]; then
        echo "Error: E2E_VOICE_STT_KEYWORDS_MIN ($min_req) > number of specs ($n_specs)."
        exit 1
    fi

    echo "  Specs ($n_specs): $specs_csv — require $min_req match(es)"
    run_one_segment "Single clip" "$transcript" "$specs_csv" "$min_req"

    echo ""
    echo "[e2e-voice-stt] Pass — single-clip TTS→STT met N-of-M."
}

run_gateway_ws() {
    local hbin port cfg ws_url gw_text norm
    hbin=$(detect_human_bin) || exit 1
    port="${E2E_VOICE_STT_GATEWAY_PORT:-3009}"
    E2E_HOME_GATEWAY=$(mktemp -d)
    mkdir -p "$E2E_HOME_GATEWAY/.human"
    cfg="$E2E_HOME_GATEWAY/.human/config.json"

    jq -n \
        --arg key "$CARTESIA_API_KEY" \
        --arg stt "$STT_MODEL" \
        --argjson port "$port" \
        '{
      memory: {backend: "sqlite"},
      voice: {stt_provider: "cartesia", stt_model: $stt},
      providers: [{name: "cartesia", api_key: $key}],
      gateway: {enabled: true, host: "127.0.0.1", port: $port, require_pairing: false}
    }' >"$cfg"

    echo ""
    echo "[e2e-voice-stt] Gateway — start human + voice.transcribe (port $port)"
    HOME="$E2E_HOME_GATEWAY" CARTESIA_API_KEY="$CARTESIA_API_KEY" "$hbin" gateway >>"$E2E_HOME_GATEWAY/gateway.log" 2>&1 &
    GW_PID=$!

    local i
    for i in $(seq 1 30); do
        if curl -sf "http://127.0.0.1:${port}/health" >/dev/null 2>&1; then
            echo "  Gateway healthy after ${i}s (pid $GW_PID)"
            break
        fi
        if ! kill -0 "$GW_PID" 2>/dev/null; then
            echo "Error: gateway exited early. Log:"
            tail -80 "$E2E_HOME_GATEWAY/gateway.log" || true
            exit 1
        fi
        sleep 1
    done
    if ! curl -sf "http://127.0.0.1:${port}/health" >/dev/null 2>&1; then
        echo "Error: gateway not healthy within 30s"
        tail -80 "$E2E_HOME_GATEWAY/gateway.log" || true
        exit 1
    fi

    cartesia_tts_wav "Gateway check: hello from the websocket voice path." "$WAV" || exit 1
    ws_url="ws://127.0.0.1:${port}/ws"
    gw_text=$(
        E2E_GATEWAY_WS="$ws_url" node "$REPO_ROOT/scripts/e2e-voice-stt-gateway.mjs" "$WAV" "$ws_url" |
            jq -r '.text // empty'
    ) || true
    if [ -z "$gw_text" ]; then
        echo "Error: gateway voice.transcribe returned empty .text"
        tail -40 "$E2E_HOME_GATEWAY/gateway.log" || true
        exit 1
    fi
    echo "  Gateway transcript: $gw_text"
    norm=$(normalize_for_match "$gw_text")
    if [[ "$norm" != *"hello"* ]]; then
        echo 'Error: gateway transcript missing "hello"'
        exit 1
    fi
    if [[ "$norm" != *"websocket"* ]] && [[ "$norm" != *"web socket"* ]]; then
        echo 'Error: gateway transcript missing "websocket" (or "web socket")'
        exit 1
    fi
    echo "  OK: gateway path contains expected keywords"

    kill "$GW_PID" 2>/dev/null || true
    wait "$GW_PID" 2>/dev/null || true
    GW_PID=""
    rm -rf "$E2E_HOME_GATEWAY"
    E2E_HOME_GATEWAY=""

    echo "[e2e-voice-stt] Pass — gateway voice.transcribe round-trip."
}

run_one_matrix_cell() {
    TTS_MODEL="$1"
    STT_MODEL="$2"
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "[e2e-voice-stt] Matrix cell: TTS=$TTS_MODEL  STT=$STT_MODEL"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    case "$E2E_MODE" in
        multi) run_multi ;;
        single) run_single ;;
        *)
            echo "Error: E2E_VOICE_STT_MODE must be multi or single (got $E2E_MODE)."
            exit 1
            ;;
    esac

    if [ "${E2E_VOICE_STT_GATEWAY:-0}" = "1" ]; then
        run_gateway_ws
    fi
}

fail=0
for tm in "${TTS_MATRIX[@]}"; do
    for sm in "${STT_MATRIX[@]}"; do
        run_one_matrix_cell "$tm" "$sm" || fail=1
    done
done

if [ "$fail" -ne 0 ]; then
    echo ""
    echo "Error: one or more matrix cells failed."
    exit 1
fi

echo ""
echo "[e2e-voice-stt] All matrix cells passed."
