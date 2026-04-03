#ifndef HU_TTS_TRANSCRIPT_PREP_H
#define HU_TTS_TRANSCRIPT_PREP_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

#define HU_PREP_MAX_SENTENCES 64
#define HU_PREP_MAX_OUTPUT    8192

typedef struct hu_prep_config {
    const char *incoming_msg;
    size_t incoming_msg_len;
    const char *default_emotion;
    float base_speed;
    float pause_factor;
    float discourse_rate;
    bool nonverbals_enabled;
    uint32_t seed;
    uint8_t hour_local;
} hu_prep_config_t;

typedef struct hu_prep_sentence {
    const char *text;
    size_t len;
    const char *emotion;
    float speed_ratio;
} hu_prep_sentence_t;

typedef struct hu_prep_result {
    char output[HU_PREP_MAX_OUTPUT];
    size_t output_len;
    hu_prep_sentence_t sentences[HU_PREP_MAX_SENTENCES];
    size_t sentence_count;
    const char *dominant_emotion;
    float volume;
} hu_prep_result_t;

/*
 * Preprocess a transcript for Cartesia TTS with SSML annotations.
 *
 * Splits text into sentences, assigns per-sentence emotion, injects
 * SSML <break>, <speed>, and <emotion> tags, weaves discourse markers,
 * and computes volume from emotional context.
 *
 * The output buffer in `result` contains the SSML-annotated transcript
 * ready to send to Cartesia's /tts/bytes endpoint.
 */
hu_error_t hu_transcript_prep(const char *transcript, size_t transcript_len,
                              const hu_prep_config_t *config, hu_prep_result_t *result);

/*
 * Segment text into sentences. Respects abbreviations (Mr., Dr., etc.)
 * and avoids splitting on decimal numbers or ellipsis.
 * Returns sentence count. Sentences point into the original text buffer.
 */
size_t hu_transcript_segment(const char *text, size_t text_len,
                             hu_prep_sentence_t *out, size_t max_sentences);

/*
 * Map emotion enum to a volume multiplier.
 * Calm/empathetic -> softer (0.85-0.9), excited/urgent -> louder (1.1-1.2).
 */
float hu_emotion_to_volume(const char *emotion);

#endif /* HU_TTS_TRANSCRIPT_PREP_H */
