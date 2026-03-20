#ifndef HU_VOICE_LOCAL_TTS_H
#define HU_VOICE_LOCAL_TTS_H

#include "human/core/allocator.h"
#include "human/core/error.h"

typedef struct hu_local_tts_config {
    const char *endpoint; /* e.g. "http://localhost:8880/v1/audio/speech" */
    const char *model;    /* NULL = omit from JSON */
    const char *voice;    /* NULL = omit from JSON */
} hu_local_tts_config_t;

hu_error_t hu_local_tts_synthesize(hu_allocator_t *alloc, const hu_local_tts_config_t *config,
                                   const char *text, char **out_path);

#endif
