#ifndef HU_VOICE_GEMINI_LIVE_H
#define HU_VOICE_GEMINI_LIVE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/voice/realtime.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Gemini Live — real-time bidirectional voice via Google's Multimodal Live API.
 *
 * Protocol: stateful WebSocket (WSS) to BidiGenerateContent endpoint.
 * Audio in:  raw 16-bit PCM, 16 kHz, little-endian
 * Audio out: raw 16-bit PCM, 24 kHz, little-endian
 *
 * Supports: barge-in, tool use, audio transcription, affective dialog.
 * Auth: API key (?key=) for dev, ADC OAuth2 bearer token for Vertex AI.
 */

typedef enum hu_gemini_live_thinking {
    HU_GL_THINKING_DEFAULT = 0,   /* server default */
    HU_GL_THINKING_NONE,          /* no thinking */
    HU_GL_THINKING_MINIMAL,       /* lowest latency (recommended for voice) */
    HU_GL_THINKING_LOW,
    HU_GL_THINKING_MEDIUM,
    HU_GL_THINKING_HIGH,
} hu_gemini_live_thinking_t;

typedef struct hu_gemini_live_config {
    const char *api_key;          /* Google AI API key (mutually exclusive with access_token) */
    const char *access_token;     /* Vertex AI OAuth2 bearer token from ADC */
    const char *model;            /* e.g. "gemini-3.1-flash-live-preview"; NULL = default */
    const char *voice;            /* voice name: Puck, Charon, Kore, Fenrir, Aoede, etc. */
    const char *system_instruction; /* system prompt; NULL = none */
    const char *region;           /* Vertex AI region; NULL = use Google AI endpoint */
    const char *project_id;       /* Vertex AI project; required when region is set */
    const char *tools_json;       /* pre-built JSON array of functionDeclarations; NULL = no tools */
    int sample_rate_in;           /* input sample rate; 0 = 16000 */
    int sample_rate_out;          /* output sample rate; 0 = 24000 */
    bool transcribe_input;        /* request input audio transcription */
    bool transcribe_output;       /* request output audio transcription */
    bool affective_dialog;        /* enable affective dialog (tone matching) */
    bool manual_vad;              /* disable server VAD; caller manages activityStart/End */
    bool enable_session_resumption; /* request sessionResumptionUpdate tokens from server */
    hu_gemini_live_thinking_t thinking_level;
} hu_gemini_live_config_t;

typedef struct hu_gemini_live_session {
    hu_allocator_t *alloc;
    hu_gemini_live_config_t config;
    bool connected;
    bool setup_sent;
    bool activity_active;         /* true between activityStart and activityEnd */
    void *ws_client;              /* hu_ws_client_t * when connected */
    char *resumption_handle;      /* latest session resumption token from server */
    size_t resumption_handle_len;
#if HU_IS_TEST
    unsigned test_recv_seq;
#endif
} hu_gemini_live_session_t;

hu_error_t hu_gemini_live_session_create(hu_allocator_t *alloc,
                                         const hu_gemini_live_config_t *config,
                                         hu_gemini_live_session_t **out);

hu_error_t hu_gemini_live_connect(hu_gemini_live_session_t *session);

hu_error_t hu_gemini_live_send_audio(hu_gemini_live_session_t *session,
                                     const void *pcm16, size_t len);

hu_error_t hu_gemini_live_send_text(hu_gemini_live_session_t *session,
                                    const char *text, size_t text_len);

/*
 * Manual VAD signaling (requires config.manual_vad = true).
 *
 * activityStart  — user began speaking; audio send is permitted after this.
 * activityEnd    — user stopped speaking; audio send is blocked until next activityStart.
 *                  On 3.1, do NOT send periodic end/start to keep alive — the model
 *                  interprets activityEnd as "user is done" and responds prematurely.
 * audioStreamEnd — flush server-side audio cache when pausing the mic (e.g. during
 *                  model playback). Required to prevent 1011 keepalive timeouts.
 */
hu_error_t hu_gemini_live_send_activity_start(hu_gemini_live_session_t *session);
hu_error_t hu_gemini_live_send_activity_end(hu_gemini_live_session_t *session);
hu_error_t hu_gemini_live_send_audio_stream_end(hu_gemini_live_session_t *session);

hu_error_t hu_gemini_live_recv_event(hu_gemini_live_session_t *session,
                                     hu_allocator_t *alloc,
                                     hu_voice_rt_event_t *out, int timeout_ms);

hu_error_t hu_gemini_live_add_tool(hu_gemini_live_session_t *session,
                                   const char *name, const char *description,
                                   const char *parameters_json);

hu_error_t hu_gemini_live_send_tool_response(hu_gemini_live_session_t *session,
                                             const char *name, const char *call_id,
                                             const char *response_json);

/*
 * Reconnect using the stored session resumption handle.
 * Call after a 1011 disconnect to resume the conversation.
 * Returns HU_ERR_NOT_FOUND if no resumption handle is stored.
 * Caller should implement exponential backoff (1s, 2s, 4s) around this call.
 */
hu_error_t hu_gemini_live_reconnect(hu_gemini_live_session_t *session);

/*
 * Build the BidiGenerateContentSetup JSON message.
 * Exposed for testability — callers should use connect/reconnect instead.
 * On success, *out_json is allocated via alloc and must be freed by caller.
 * If resumption_handle is non-NULL, it is included in sessionResumption.
 */
hu_error_t hu_gemini_live_build_setup_json(hu_allocator_t *alloc,
                                           const hu_gemini_live_config_t *config,
                                           const char *resumption_handle,
                                           char **out_json, size_t *out_len);

void hu_gemini_live_session_destroy(hu_gemini_live_session_t *session);

#endif
