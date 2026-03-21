#ifndef HU_COGNITION_METACOGNITION_H
#define HU_COGNITION_METACOGNITION_H

#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_metacog_action {
    HU_METACOG_ACTION_NONE,
    HU_METACOG_ACTION_REFLECT,
    HU_METACOG_ACTION_DEEPEN,
    HU_METACOG_ACTION_SIMPLIFY,
    HU_METACOG_ACTION_CLARIFY,
    HU_METACOG_ACTION_SWITCH_STRATEGY,
} hu_metacog_action_t;

#define HU_METACOG_SIGNAL_RING_SIZE 8

typedef struct hu_metacognition_signal {
    float confidence;           /* 0.0–1.0; estimated from hedging word frequency */
    float coherence;            /* 0.0–1.0; semantic overlap with user query */
    float repetition;           /* 0.0–1.0; n-gram overlap with prior assistant turns */
    float token_efficiency;     /* response_tokens / input_tokens ratio */
    float emotional_alignment;  /* from emotional cognition confidence */
} hu_metacognition_signal_t;

typedef struct hu_metacognition {
    hu_metacognition_signal_t signals[HU_METACOG_SIGNAL_RING_SIZE];
    size_t signal_count;
    size_t signal_idx;
    hu_metacog_action_t last_action;
    uint32_t reflect_count;
    uint32_t max_reflects;
    /* Thresholds (tunable) */
    float confidence_threshold;     /* below this -> CLARIFY or REFLECT */
    float coherence_threshold;      /* below this -> SWITCH_STRATEGY */
    float repetition_threshold;     /* above this -> SIMPLIFY */
} hu_metacognition_t;

/* Initialize metacognition state with default thresholds. */
void hu_metacognition_init(hu_metacognition_t *mc);

/* Monitor: compute signals from a model response.
 * user_query / response / prev_response are the raw text;
 * emotional_confidence is from hu_emotional_cognition_t.confidence.
 * input_tokens / output_tokens are from the provider response. */
hu_metacognition_signal_t hu_metacognition_monitor(
    const char *user_query, size_t user_query_len,
    const char *response, size_t response_len,
    const char *prev_response, size_t prev_response_len,
    float emotional_confidence,
    uint64_t input_tokens, uint64_t output_tokens);

/* Analyze signals and plan a control action. */
hu_metacog_action_t hu_metacognition_plan_action(hu_metacognition_t *mc,
                                                  const hu_metacognition_signal_t *signal);

/* Record signal in the ring buffer. */
void hu_metacognition_record_signal(hu_metacognition_t *mc,
                                    const hu_metacognition_signal_t *signal);

/* Get the name string for an action (for observer events). */
const char *hu_metacog_action_name(hu_metacog_action_t action);

#endif /* HU_COGNITION_METACOGNITION_H */
