#ifndef HU_VOICE_SESSION_H
#define HU_VOICE_SESSION_H

#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/voice/duplex.h"
#include "human/voice/realtime.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Unified duplex FSM + optional OpenAI Realtime WebSocket session. */
typedef struct hu_voice_session {
    hu_duplex_session_t duplex;
    hu_voice_rt_session_t *rt;
    bool active;
    int64_t started_at;
    int64_t last_audio_ms;
    char channel_name[32];
} hu_voice_session_t;

hu_error_t hu_voice_session_start(hu_allocator_t *alloc, hu_voice_session_t *session,
                                  const char *channel_name, size_t channel_name_len,
                                  const hu_config_t *config);
hu_error_t hu_voice_session_stop(hu_voice_session_t *session);
hu_error_t hu_voice_session_send_audio(hu_voice_session_t *session, const uint8_t *pcm16,
                                       size_t pcm16_len);
hu_error_t hu_voice_session_on_interrupt(hu_voice_session_t *session);

#endif
