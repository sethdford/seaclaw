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

typedef enum hu_metacog_difficulty {
    HU_METACOG_DIFFICULTY_EASY,
    HU_METACOG_DIFFICULTY_MEDIUM,
    HU_METACOG_DIFFICULTY_HARD,
} hu_metacog_difficulty_t;

#define HU_METACOG_SIGNAL_RING_SIZE 8
#define HU_METACOG_TRACE_ID_CAP 40 /* UUID + NUL + margin */

/* Tunable policy (config.json "agent.metacognition" + env overrides). */
typedef struct hu_metacog_settings {
    bool enabled;
    float confidence_threshold;
    float coherence_threshold;
    float repetition_threshold;
    uint32_t max_reflects;
    uint32_t max_regen;
    uint32_t hysteresis_min;
    bool use_calibrated_risk;
    /* If mean weighted risk >= threshold, hysteresis_min is treated as max(1, min-1). */
    float risk_high_threshold;
    /* Non-negative; internally normalized when computing risk in [0,1]. */
    float w_low_confidence;
    float w_low_coherence;
    float w_repetition;
    float w_stuck;
    float w_low_satisfaction;
    float w_low_trajectory;
} hu_metacog_settings_t;

typedef struct hu_metacognition_signal {
    float confidence;           /* 0.0–1.0; estimated from hedging word frequency */
    float coherence;            /* 0.0–1.0; semantic overlap with user query */
    float repetition;         /* 0.0–1.0; n-gram overlap with prior assistant turns */
    float token_efficiency;   /* response_tokens / input_tokens ratio */
    float emotional_alignment; /* from emotional cognition confidence */
    float stuck_score;        /* 0.0–1.0; repetition + rolling repetition pressure */
    float satisfaction_proxy; /* 0.0–1.0; response length vs expected band */
    float trajectory_confidence; /* 0.0–1.0; decay-weighted composite of ring (filled in plan) */
} hu_metacognition_signal_t;

typedef struct hu_metacog_trend {
    float confidence_slope;
    float coherence_slope;
    bool is_degrading;
} hu_metacog_trend_t;

typedef struct hu_metacognition {
    hu_metacog_settings_t cfg;
    hu_metacognition_signal_t signals[HU_METACOG_SIGNAL_RING_SIZE];
    size_t signal_count;
    size_t signal_idx;
    hu_metacog_action_t last_action;
    uint32_t reflect_count;
    uint32_t regen_count;
    hu_metacog_difficulty_t difficulty;
    uint32_t consecutive_bad_count;
    char pending_outcome_trace_id[HU_METACOG_TRACE_ID_CAP];
    bool last_suppressed_hysteresis; /* set when plan_action returns NONE due to hysteresis gate */
} hu_metacognition_t;

/* Default policy in *out (all fields set). */
void hu_metacog_settings_default(hu_metacog_settings_t *out);

/* Initialize metacognition state with default cfg. */
void hu_metacognition_init(hu_metacognition_t *mc);

/* Replace mc->cfg from loaded config (bootstrap / tests). */
void hu_metacognition_apply_config(hu_metacognition_t *mc, const hu_metacog_settings_t *settings);

/* Weighted [0,1] “bad turn” score from current signal (trajectory_confidence should be set). */
float hu_metacog_calibrated_risk(const hu_metacognition_t *mc,
                                 const hu_metacognition_signal_t *signal);

/* Heuristic difficulty from user message (length, punctuation, code/math hints). */
hu_metacog_difficulty_t hu_metacog_estimate_difficulty(const char *msg, size_t msg_len);

const char *hu_metacog_difficulty_name(hu_metacog_difficulty_t d);

/* Least-squares slopes over the signal ring (needs at least 2 samples for non-zero slopes). */
hu_metacog_trend_t hu_metacog_compute_trend(const hu_metacognition_t *mc);

/* Decay-weighted [0,1] composite over stored signals (recent entries weighted higher). */
float hu_metacog_trajectory_confidence(const hu_metacognition_t *mc);

/* Monitor: compute signals from a model response.
 * mc_opt may be NULL; when set, stuck_score uses rolling repetition from the ring. */
hu_metacognition_signal_t hu_metacognition_monitor(
    const char *user_query, size_t user_query_len,
    const char *response, size_t response_len,
    const char *prev_response, size_t prev_response_len,
    float emotional_confidence,
    uint64_t input_tokens, uint64_t output_tokens,
    hu_metacognition_t *mc_opt);

/* Analyze signals and plan a control action. Mutates *signal to set trajectory_confidence. */
hu_metacog_action_t hu_metacognition_plan_action(hu_metacognition_t *mc,
                                                 hu_metacognition_signal_t *signal);

void hu_metacognition_record_signal(hu_metacognition_t *mc,
                                  const hu_metacognition_signal_t *signal);

hu_error_t hu_metacognition_apply(hu_metacog_action_t action, char *prompt_buf, size_t prompt_cap,
                                  size_t *prompt_len_out);

float hu_metacog_label_from_followup(const char *followup, size_t followup_len);

const char *hu_metacog_action_name(hu_metacog_action_t action);

#endif /* HU_COGNITION_METACOGNITION_H */
