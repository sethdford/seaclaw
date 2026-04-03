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

/* Optional extras for the voice provider factory (all fields may be NULL/0). */
typedef struct hu_voice_provider_extras {
    const char *system_instruction;  /* persona/system prompt for Gemini Live */
    const char *tools_json;          /* JSON array of tool declarations for Gemini Live setup */
    const char *voice_id;            /* override voice (NULL = use config default) */
    const char *model_id;            /* override model (NULL = use config default) */
    const char *api_key;             /* override API key (NULL = use config lookup) */
    const char *vertex_region;       /* Vertex AI region (NULL = use config default) */
    const char *vertex_project;      /* Vertex AI project (NULL = use config default) */
    const char *vertex_access_token; /* Vertex AI access token (NULL = use config default) */
    int sample_rate;                 /* audio sample rate (0 = use default 24000) */
} hu_voice_provider_extras_t;

/*
 * High-level factory: create a voice provider from hu_config_t + mode string.
 * Handles API key lookup, model/voice defaults, and backend-specific config.
 * Mode: "gemini_live", "openai_realtime", "realtime". Returns HU_ERR_NOT_SUPPORTED for unknown.
 * extras may be NULL. Does NOT call connect() — caller must connect after creation.
 */
struct hu_config;
hu_error_t hu_voice_provider_create_from_config(hu_allocator_t *alloc,
                                                const struct hu_config *config, const char *mode,
                                                const hu_voice_provider_extras_t *extras,
                                                hu_voice_provider_t *out);

/*
 * Lightweight factory: create a voice provider from mode string + extras only.
 * Does not require hu_config_t — gets api_key, model, voice from extras fields.
 * For use by channels/modules that don't have access to the global config.
 */
hu_error_t hu_voice_provider_create_from_extras(hu_allocator_t *alloc, const char *mode,
                                                const hu_voice_provider_extras_t *extras,
                                                hu_voice_provider_t *out);

#endif
