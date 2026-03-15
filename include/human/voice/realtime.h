#ifndef HU_VOICE_REALTIME_H
#define HU_VOICE_REALTIME_H
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
typedef struct hu_voice_rt_config {
    char *base_url;   /* e.g. wss://api.openai.com/v1/realtime; NULL = default */
    char *model;
    char *voice;
    int sample_rate;
    bool vad_enabled;
} hu_voice_rt_config_t;
typedef struct hu_voice_rt_session {
    hu_allocator_t *alloc;
    hu_voice_rt_config_t config;
    bool connected;
    char *session_id;
    void *ws_client; /* hu_ws_client_t * when connected; opaque */
} hu_voice_rt_session_t;
hu_error_t hu_voice_rt_session_create(hu_allocator_t *alloc, const hu_voice_rt_config_t *config, hu_voice_rt_session_t **out);
hu_error_t hu_voice_rt_connect(hu_voice_rt_session_t *session);
hu_error_t hu_voice_rt_send_audio(hu_voice_rt_session_t *session, const void *data, size_t data_len);
void hu_voice_rt_session_destroy(hu_voice_rt_session_t *session);
#endif
