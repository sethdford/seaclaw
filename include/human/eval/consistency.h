#ifndef HU_EVAL_CONSISTENCY_H
#define HU_EVAL_CONSISTENCY_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Personality consistency metrics — TwinVoice-inspired evaluation.
 *
 * Three metrics for measuring persona fidelity across conversations:
 * 1. Prompt-to-line: does each response line match the persona prompt?
 * 2. Line-to-line: are consecutive responses stylistically consistent?
 * 3. Q&A consistency: does the agent answer the same question the same way?
 *
 * Plus drift detection: track metrics over time, alert when outside bounds.
 */

typedef struct hu_consistency_metrics {
    float prompt_alignment;     /* 0.0–1.0: response matches persona prompt */
    float line_consistency;     /* 0.0–1.0: consecutive responses are consistent */
    float qa_stability;         /* 0.0–1.0: same question yields same-flavored answer */
    float lexical_fidelity;     /* 0.0–1.0: vocabulary matches persona preferred/avoided */
    float composite;            /* weighted average of all metrics */
} hu_consistency_metrics_t;

typedef struct hu_drift_detector {
    hu_consistency_metrics_t baseline;
    hu_consistency_metrics_t current;
    float drift_threshold;      /* alert when |current - baseline| > threshold */
    size_t sample_count;
    bool baseline_set;
    bool drift_detected;
} hu_drift_detector_t;

/* Score prompt alignment: how well does a response match persona traits/vocab. */
hu_error_t hu_consistency_score_prompt_alignment(
    const char *response, size_t response_len,
    const char *const *traits, size_t traits_count,
    const char *const *preferred_vocab, size_t preferred_count,
    const char *const *avoided_vocab, size_t avoided_count,
    float *score);

/* Score line-to-line consistency between two consecutive responses. */
hu_error_t hu_consistency_score_line(
    const char *prev_response, size_t prev_len,
    const char *curr_response, size_t curr_len,
    float *score);

/* Score Q&A stability: compare two answers to the same question. */
hu_error_t hu_consistency_score_qa(
    const char *answer_a, size_t a_len,
    const char *answer_b, size_t b_len,
    float *score);

/* Score lexical fidelity against persona vocabulary. */
hu_error_t hu_consistency_score_lexical(
    const char *response, size_t response_len,
    const char *const *preferred, size_t preferred_count,
    const char *const *avoided, size_t avoided_count,
    float *score);

/* Compute composite from individual metrics. */
float hu_consistency_composite(const hu_consistency_metrics_t *m);

/* Initialize drift detector with threshold. */
void hu_drift_detector_init(hu_drift_detector_t *d, float threshold);

/* Set baseline from current metrics snapshot. */
void hu_drift_detector_set_baseline(hu_drift_detector_t *d,
                                    const hu_consistency_metrics_t *metrics);

/* Update with new observation; returns true if drift detected. */
bool hu_drift_detector_update(hu_drift_detector_t *d,
                              const hu_consistency_metrics_t *metrics);

#endif
