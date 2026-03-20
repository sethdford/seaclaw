#ifndef HU_VOICE_LOCAL_STT_H
#define HU_VOICE_LOCAL_STT_H

#include "human/core/allocator.h"
#include "human/core/error.h"

typedef struct hu_local_stt_config {
    const char *endpoint; /* e.g. "http://localhost:8000/v1/audio/transcriptions" */
    const char *model;    /* NULL = default model string for the request */
    const char *language; /* NULL = omit (auto-detect) */
} hu_local_stt_config_t;

hu_error_t hu_local_stt_transcribe(hu_allocator_t *alloc, const hu_local_stt_config_t *config,
                                   const char *audio_path, char **out_text, size_t *out_len);

#endif
