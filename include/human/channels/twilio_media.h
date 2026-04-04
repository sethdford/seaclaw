#ifndef HU_TWILIO_MEDIA_H
#define HU_TWILIO_MEDIA_H

#include "human/channel.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

typedef struct hu_twilio_media_config {
    char *account_sid;
    char *auth_token;
    char *phone_number;
    char *voice_webhook_url;
    char *voice_provider; /* "gemini_live" or "openai_realtime" */
} hu_twilio_media_config_t;

hu_error_t hu_twilio_media_create(hu_allocator_t *alloc, const hu_twilio_media_config_t *cfg,
                                  hu_channel_t *out);
void hu_twilio_media_destroy(hu_channel_t *ch, hu_allocator_t *alloc);

/* Generate TwiML response for incoming voice call webhook. Caller frees *out. */
hu_error_t hu_twilio_media_twiml(hu_allocator_t *alloc, const char *stream_url, size_t stream_url_len,
                                 char **out, size_t *out_len);

/* Convert mulaw 8kHz audio to PCM 16-bit 16kHz. Caller frees *out. */
hu_error_t hu_twilio_media_mulaw_to_pcm(hu_allocator_t *alloc, const uint8_t *mulaw, size_t mulaw_len,
                                        int16_t **out, size_t *out_samples);

/* Convert PCM 16-bit 24kHz audio to mulaw 8kHz. Caller frees *out. */
hu_error_t hu_twilio_media_pcm_to_mulaw(hu_allocator_t *alloc, const int16_t *pcm, size_t pcm_samples,
                                        uint8_t **out, size_t *out_len);

#endif
