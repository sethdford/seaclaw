#ifndef HU_VOICE_EMOTION_VOICE_MAP_H
#define HU_VOICE_EMOTION_VOICE_MAP_H

#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Emotion-to-voice parameter mapping for expressive TTS.
 *
 * Maps detected emotional signals from text analysis or audio features
 * into TTS synthesis parameters (pitch, rate, emphasis, warmth).
 * Enables the agent's voice to reflect emotional context rather than
 * using a flat monotone for all responses.
 *
 * Inspired by S2S emotional prosody research (arXiv:2602.24080).
 */

typedef enum hu_voice_emotion {
    HU_VOICE_EMOTION_NEUTRAL = 0,
    HU_VOICE_EMOTION_JOY,
    HU_VOICE_EMOTION_SADNESS,
    HU_VOICE_EMOTION_EMPATHY,
    HU_VOICE_EMOTION_EXCITEMENT,
    HU_VOICE_EMOTION_CONCERN,
    HU_VOICE_EMOTION_CALM,
    HU_VOICE_EMOTION_URGENCY,
    HU_VOICE_EMOTION_COUNT,
} hu_voice_emotion_t;

typedef struct hu_voice_params {
    float pitch_shift;  /* semitones offset from baseline (-6.0 to +6.0) */
    float rate_factor;  /* multiplier on speaking rate (0.7 to 1.4) */
    float emphasis;     /* overall dynamic range (0.0 = flat, 1.0 = full expression) */
    float warmth;       /* voice timbre warmth (0.0 = clinical, 1.0 = warm) */
    float pause_factor; /* multiplier on inter-clause pauses (0.5 to 2.0) */
} hu_voice_params_t;

/** Default voice params (neutral, no modification). */
hu_voice_params_t hu_voice_params_default(void);

/** Get voice params for a given emotion class. */
hu_voice_params_t hu_emotion_voice_map(hu_voice_emotion_t emotion);

/**
 * Detect dominant emotion from text content (lightweight heuristic).
 * Uses lexical cues — not a full sentiment model.
 */
hu_error_t hu_emotion_detect_from_text(const char *text, size_t text_len,
                                       hu_voice_emotion_t *out_emotion, float *out_confidence);

/** Blend two voice param sets by a factor (0.0 = a, 1.0 = b). */
hu_voice_params_t hu_voice_params_blend(const hu_voice_params_t *a, const hu_voice_params_t *b,
                                        float factor);

/** Get human-readable name for emotion class. */
const char *hu_emotion_class_name(hu_voice_emotion_t emotion);

#endif /* HU_VOICE_EMOTION_VOICE_MAP_H */
