#ifndef HU_VOICE_PROVIDER_H
#define HU_VOICE_PROVIDER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/voice/realtime.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Voice Provider abstraction — vtable interface for swappable voice backends.
 * Implementations: OpenAI Realtime (realtime.c), Gemini Live (gemini_live.c).
 */

typedef struct hu_voice_provider_vtable {
    hu_error_t (*connect)(void *ctx);
    hu_error_t (*send_audio)(void *ctx, const void *pcm16, size_t len);
    hu_error_t (*recv_event)(void *ctx, hu_allocator_t *alloc, hu_voice_rt_event_t *out,
                             int timeout_ms);
    hu_error_t (*add_tool)(void *ctx, const char *name, const char *description,
                           const char *parameters_json);
    hu_error_t (*cancel_response)(void *ctx);
    void (*disconnect)(void *ctx, hu_allocator_t *alloc);
    const char *(*get_name)(void *ctx);
    /* Manual VAD signaling — NULL if provider doesn't support it */
    hu_error_t (*send_activity_start)(void *ctx);
    hu_error_t (*send_activity_end)(void *ctx);
    hu_error_t (*send_audio_stream_end)(void *ctx);
    /* Session resumption reconnect — NULL if provider doesn't support it */
    hu_error_t (*reconnect)(void *ctx);
    /* Tool response — NULL if provider doesn't support tool calls */
    hu_error_t (*send_tool_response)(void *ctx, const char *name, const char *call_id,
                                     const char *response_json);
} hu_voice_provider_vtable_t;

typedef struct hu_voice_provider {
    void *ctx;
    const hu_voice_provider_vtable_t *vtable;
} hu_voice_provider_t;

/* Create an OpenAI Realtime voice provider (wraps hu_voice_rt_session_t). */
hu_error_t hu_voice_provider_openai_create(hu_allocator_t *alloc,
                                           const hu_voice_rt_config_t *config,
                                           hu_voice_provider_t *out);

/* Create a Gemini Live voice provider (Multimodal Live API over WebSocket). */
struct hu_gemini_live_config;
hu_error_t hu_voice_provider_gemini_live_create(hu_allocator_t *alloc,
                                                const struct hu_gemini_live_config *config,
                                                hu_voice_provider_t *out);

#endif
