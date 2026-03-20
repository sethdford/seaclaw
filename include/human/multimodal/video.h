#ifndef HU_MULTIMODAL_VIDEO_H
#define HU_MULTIMODAL_VIDEO_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stddef.h>

/* Describe a local video file.
 * Gemini/Google providers: inline video bytes via generateContent (up to 50MB), else ffmpeg key frames + vision.
 * Supported extensions: .mp4, .mov, .webm, .avi
 * Caller owns *out_text (free with alloc). */
hu_error_t hu_multimodal_process_video(hu_allocator_t *alloc, const char *file_path, size_t path_len,
                                       hu_provider_t *provider, const char *model, size_t model_len,
                                       char **out_text, size_t *out_text_len);

#endif
