#ifndef HU_VOICE_OPUS_H
#define HU_VOICE_OPUS_H
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>
typedef struct hu_opus_encoder hu_opus_encoder_t;
typedef struct hu_opus_decoder hu_opus_decoder_t;
hu_error_t hu_opus_encoder_create(hu_allocator_t *alloc, int sample_rate, int channels,
                                  hu_opus_encoder_t **out);
hu_error_t hu_opus_encode(hu_opus_encoder_t *enc, const int16_t *pcm, size_t frame_size,
                          uint8_t *out_buf, size_t out_cap, size_t *out_len);
void hu_opus_encoder_destroy(hu_opus_encoder_t *enc, hu_allocator_t *alloc);
hu_error_t hu_opus_decoder_create(hu_allocator_t *alloc, int sample_rate, int channels,
                                  hu_opus_decoder_t **out);
hu_error_t hu_opus_decode(hu_opus_decoder_t *dec, const uint8_t *data, size_t data_len,
                          int16_t *out_pcm, size_t max_samples, size_t *out_samples);
void hu_opus_decoder_destroy(hu_opus_decoder_t *dec, hu_allocator_t *alloc);
#endif
