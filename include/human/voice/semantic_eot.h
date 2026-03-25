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

#endif /* HU_VOICE_SEMANTIC_EOT_H */
