#ifndef HU_TTS_VOICE_CLONE_H
#define HU_TTS_VOICE_CLONE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Cartesia voice cloning API client.
 *
 * Uploads an audio sample (5-10 seconds of clear speech) to Cartesia's
 * POST /voices/clone endpoint and returns a voice UUID that can be used
 * for TTS synthesis via hu_cartesia_tts_synthesize().
 *
 * Requires HU_ENABLE_CARTESIA and HU_ENABLE_CURL at build time.
 */

typedef struct hu_voice_clone_result {
    char voice_id[64];
    char name[128];
    char language[8];
    bool success;
    char error[256];
    size_t error_len;
} hu_voice_clone_result_t;

typedef struct hu_voice_clone_config {
    const char *name;
    size_t name_len;
    const char *description;
    size_t description_len;
    const char *language; /* ISO 639-1, e.g. "en"; default "en" */
    size_t language_len;
    const char *base_voice_id; /* optional refinement base */
    size_t base_voice_id_len;
} hu_voice_clone_config_t;

void hu_voice_clone_config_default(hu_voice_clone_config_t *cfg);

/* Clone a voice from a local audio file (WAV, MP3, M4A, CAF, OGG).
 * Uploads to Cartesia and returns the new voice UUID. */
hu_error_t hu_voice_clone_from_file(hu_allocator_t *alloc, const char *api_key, size_t api_key_len,
                                    const char *file_path, const hu_voice_clone_config_t *cfg,
                                    hu_voice_clone_result_t *out);

/* Clone a voice from raw audio bytes (for gateway / programmatic use).
 * mime_type: e.g. "audio/wav", "audio/mpeg". */
hu_error_t hu_voice_clone_from_bytes(hu_allocator_t *alloc, const char *api_key, size_t api_key_len,
                                     const uint8_t *audio, size_t audio_len, const char *mime_type,
                                     const hu_voice_clone_config_t *cfg,
                                     hu_voice_clone_result_t *out);

/* Write a voice_id into a persona JSON file at ~/.human/personas/<name>.json.
 * Sets voice.voice_id and voice.provider = "cartesia". */
hu_error_t hu_persona_set_voice_id(hu_allocator_t *alloc, const char *persona_name, size_t name_len,
                                   const char *voice_id, size_t voice_id_len);

#endif /* HU_TTS_VOICE_CLONE_H */
