#ifndef HU_VOICE_REALTIME_H
#define HU_VOICE_REALTIME_H
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
typedef struct hu_voice_rt_config {
    const char *base_url; /* e.g. wss://api.openai.com/v1/realtime; NULL = default */
    const char *model;
    const char *voice;
    const char *api_key; /* OpenAI API key for Authorization header (Realtime WebSocket) */
    int sample_rate;
    bool vad_enabled;
} hu_voice_rt_config_t;
typedef struct hu_voice_rt_session {
    hu_allocator_t *alloc;
    hu_voice_rt_config_t config;
    bool connected;
    char *session_id;
    void *ws_client; /* hu_ws_client_t * when connected; opaque */
#if HU_IS_TEST
    /** Mock recv_event sequence per session (alternates transcript / done). */
    unsigned test_recv_seq;
#endif
} hu_voice_rt_session_t;

typedef struct hu_voice_rt_event {
    char type[64]; /* event type: "response.audio.delta", "response.audio.done", etc. */
    char *audio_base64; /* base64-encoded audio delta (caller frees via hu_voice_rt_event_free) */
    size_t audio_base64_len;
    char *transcript; /* text transcript (caller frees) */
    size_t transcript_len;
    char *tool_call_id;
    size_t tool_call_id_len;
    char *tool_name;
    size_t tool_name_len;
    char *tool_args_json;
    size_t tool_args_json_len;
    bool error;               /* true if this is an error event */
    int go_away_ms;           /* goAway timeout hint (0 = none) */
    bool done;                /* true if this is an end-of-response event */
    bool interrupted;         /* response was interrupted by user */
    bool generation_complete; /* model finished generating */
} hu_voice_rt_event_t;

hu_error_t hu_voice_rt_session_create(hu_allocator_t *alloc, const hu_voice_rt_config_t *config,
                                      hu_voice_rt_session_t **out);
hu_error_t hu_voice_rt_connect(hu_voice_rt_session_t *session);
hu_error_t hu_voice_rt_send_audio(hu_voice_rt_session_t *session, const void *data, size_t data_len);
/** Cancel the in-flight model response (OpenAI Realtime `response.cancel`). */
hu_error_t hu_voice_rt_response_cancel(hu_voice_rt_session_t *session);
hu_error_t hu_voice_rt_recv_event(hu_voice_rt_session_t *session, hu_allocator_t *alloc,
                                  hu_voice_rt_event_t *out, int timeout_ms);
void hu_voice_rt_event_free(hu_allocator_t *alloc, hu_voice_rt_event_t *event);
hu_error_t hu_voice_rt_add_tool(hu_voice_rt_session_t *session, const char *name,
                                const char *description, const char *parameters_json);
void hu_voice_rt_session_destroy(hu_voice_rt_session_t *session);
#endif
