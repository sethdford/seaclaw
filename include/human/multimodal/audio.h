#ifndef HU_MULTIMODAL_AUDIO_H
#define HU_MULTIMODAL_AUDIO_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stddef.h>

/* Transcribe a local audio file via the provider (e.g. OpenAI transcriptions).
 * Supported extensions: .mp3, .wav, .ogg, .m4a, .caf
 * Caller owns *out_text (free with alloc). */
hu_error_t hu_multimodal_process_audio(hu_allocator_t *alloc, const char *file_path, size_t path_len,
                                       hu_provider_t *provider, const char *model, size_t model_len,
                                       char **out_text, size_t *out_text_len);

#endif
