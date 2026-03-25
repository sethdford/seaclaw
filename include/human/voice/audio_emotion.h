#ifndef HU_VOICE_AUDIO_EMOTION_H
#define HU_VOICE_AUDIO_EMOTION_H

#include "human/core/error.h"
#include "human/voice/emotion_voice_map.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Audio emotion detection from STT features.
 *
 * Extends the text-based emotion detection (emotion_voice_map.h) with
 * acoustic features: energy level, pitch variance, speaking rate.
 * These are computed from PCM audio and fused with text-based emotion
 * to produce a more accurate emotion classification.
 */

typedef struct hu_audio_features {
    float energy_db;     /* RMS energy in decibels */
    float pitch_mean_hz; /* mean fundamental frequency */
    float pitch_var_hz;  /* pitch variance (monotone vs. expressive) */
    float speaking_rate; /* estimated syllables per second */
    float silence_ratio; /* fraction of audio that is silence */
    bool valid;          /* true if features were successfully extracted */
} hu_audio_features_t;

/* Extract acoustic features from PCM audio (16-bit signed, mono). */
hu_error_t hu_audio_features_extract(const int16_t *pcm, size_t sample_count, uint32_t sample_rate,
                                     hu_audio_features_t *out);

/* Extract acoustic features from float PCM (32-bit float, mono). */
hu_error_t hu_audio_features_extract_f32(const float *pcm, size_t sample_count,
                                         uint32_t sample_rate, hu_audio_features_t *out);

/* Classify emotion from audio features alone. */
hu_error_t hu_audio_emotion_classify(const hu_audio_features_t *features,
                                     hu_voice_emotion_t *out_emotion, float *out_confidence);

/* Fuse text-based and audio-based emotion detection for final classification. */
hu_error_t hu_emotion_fuse(hu_voice_emotion_t text_emotion, float text_confidence,
                           hu_voice_emotion_t audio_emotion, float audio_confidence,
                           hu_voice_emotion_t *out_emotion, float *out_confidence);

#endif /* HU_VOICE_AUDIO_EMOTION_H */
