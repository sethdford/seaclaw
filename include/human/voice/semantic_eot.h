#ifndef HU_VOICE_SEMANTIC_EOT_H
#define HU_VOICE_SEMANTIC_EOT_H

#include "human/core/error.h"
#include "human/voice/duplex.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Semantic End-of-Turn (EOT) detection.
 *
 * Replaces pure silence-timeout-based turn boundaries with text-semantic
 * analysis. Inspired by Phoenix-VAD (arXiv:2509.20410) and the Hierarchical
 * EOT model (arXiv:2603.13379).
 *
 * Uses a lightweight heuristic cascade:
 *   1. Syntactic completeness — sentence-ending punctuation, complete clauses
 *   2. Pragmatic cues — question marks trigger agent, trailing ellipsis holds
 *   3. Lexical turn signals — explicit yielding phrases ("what do you think?")
 *   4. Length/pause correlation — short utterance + pause = likely complete
 *
 * This is the heuristic path for when no LLM turn predictor is available.
 * When an LLM predictor exists, it emits control tokens directly.
 */

typedef enum hu_eot_state {
    HU_EOT_COMPLETE = 0,
    HU_EOT_INCOMPLETE,
    HU_EOT_BACKCHANNEL,
    HU_EOT_HOLD,
} hu_eot_state_t;

typedef struct hu_semantic_eot_config {
    uint32_t min_utterance_chars; /* require at least N chars before EOT (default: 2) */
    uint32_t pause_threshold_ms;  /* silence after text before triggering (default: 400) */
    double confidence_threshold;  /* minimum confidence to declare EOT (default: 0.6) */
} hu_semantic_eot_config_t;

typedef struct hu_semantic_eot_result {
    bool is_endpoint;
    double confidence; /* [0.0, 1.0] */
    hu_turn_signal_t suggested_signal;
    hu_eot_state_t predicted_state;
} hu_semantic_eot_result_t;

/** Initialize config with sensible defaults. */
void hu_semantic_eot_config_default(hu_semantic_eot_config_t *cfg);

/**
 * Analyze accumulated transcript text to determine if it represents a
 * complete turn (semantic endpoint).
 *
 * text: the accumulated transcript so far for this micro-turn burst.
 * silence_ms: milliseconds of silence since last speech.
 */
hu_error_t hu_semantic_eot_analyze(const hu_semantic_eot_config_t *cfg, const char *text,
                                   size_t text_len, uint32_t silence_ms,
                                   hu_semantic_eot_result_t *out);

/**
 * Semantic EOT with acoustic features (energy / pitch trend) fused with text heuristics.
 * energy_db: RMS-derived dB from hu_audio_features_t; pitch_delta: change vs. baseline (Hz).
 */
hu_error_t hu_semantic_eot_analyze_with_audio(const hu_semantic_eot_config_t *cfg, const char *text,
                                              size_t text_len, uint32_t silence_ms, float energy_db,
                                              float pitch_delta, hu_semantic_eot_result_t *out);

/**
 * Feature-based EOT classifier using logistic regression over engineered features.
 * Replaces pure heuristic cascades with a learned linear model for higher accuracy.
 * Feature vector: [syntax_complete, question_mark, ellipsis, yield_phrase, backchannel,
 *                  hold_filler, word_count_norm, silence_norm, energy_norm, pitch_norm]
 *
 * Weights can be updated by calling hu_semantic_eot_set_weights (e.g. from config or
 * fine-tuning). Default weights are calibrated from conversational corpora.
 */

#define HU_EOT_FEATURE_DIM 10

typedef struct hu_semantic_eot_classifier {
    float weights[HU_EOT_FEATURE_DIM];
    float bias;
    float threshold; /* sigmoid output above this → endpoint (default: 0.55) */
} hu_semantic_eot_classifier_t;

void hu_semantic_eot_classifier_default(hu_semantic_eot_classifier_t *cls);

/** Set custom weights (e.g. from fine-tuning on user conversation data). */
hu_error_t hu_semantic_eot_set_weights(hu_semantic_eot_classifier_t *cls, const float *weights,
                                       size_t dim, float bias, float threshold);

/** Extract feature vector from text + audio. out_features must have HU_EOT_FEATURE_DIM elements. */
hu_error_t hu_semantic_eot_extract_features(const char *text, size_t text_len, uint32_t silence_ms,
                                            float energy_db, float pitch_delta,
                                            float *out_features);

/** Classify using the learned model. Falls back to heuristic path if cls is NULL. */
hu_error_t hu_semantic_eot_classify(const hu_semantic_eot_classifier_t *cls,
                                    const hu_semantic_eot_config_t *cfg, const char *text,
                                    size_t text_len, uint32_t silence_ms, float energy_db,
                                    float pitch_delta, hu_semantic_eot_result_t *out);

#endif /* HU_VOICE_SEMANTIC_EOT_H */
