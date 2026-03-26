#ifndef HU_TTS_AUDIO_PIPELINE_H
#define HU_TTS_AUDIO_PIPELINE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

/* Write MP3 bytes to temp file, convert to CAF for iMessage.
 * Returns path to .caf file (or .mp3 on fallback). Caller must call hu_audio_cleanup_temp when
 * done. On failure, returns error and out_audio_path is unchanged. */
hu_error_t hu_audio_mp3_to_caf(hu_allocator_t *alloc, const unsigned char *mp3_bytes,
                               size_t mp3_len, char *out_audio_path, size_t out_path_cap);

/* Clean up temp file. Call when done sending. */
void hu_audio_cleanup_temp(const char *audio_path);

/* Write raw audio bytes to a temp file. file_ext_no_dot must be "mp3" or "wav".
 * Caller must hu_audio_cleanup_temp(out_path) when done. */
hu_error_t hu_audio_tts_bytes_to_temp(hu_allocator_t *alloc, const unsigned char *bytes,
                                      size_t bytes_len, const char *file_ext_no_dot, char *out_path,
                                      size_t out_cap);

/* Generic pipeline process. Returns HU_ERR_NOT_SUPPORTED when Cartesia is not enabled. */
hu_error_t hu_audio_pipeline_process(hu_allocator_t *alloc, const void *input, size_t input_len,
                                     void **out, size_t *out_len);

#endif
