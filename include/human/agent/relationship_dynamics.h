#ifndef HU_AGENT_RELATIONSHIP_DYNAMICS_H
#define HU_AGENT_RELATIONSHIP_DYNAMICS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Relationship Dynamics — continuous velocity/drift/repair model.
 * Unlike persona/relationship.h (session-based warmth stages),
 * this models rate-of-change in closeness over rolling windows.
 */

typedef enum hu_reldyn_mode {
    HU_RELDYN_NORMAL = 0,
    HU_RELDYN_DEEPENING,
    HU_RELDYN_DRIFTING,
    HU_RELDYN_REPAIR,
    HU_RELDYN_RECONNECTING
} hu_reldyn_mode_t;

typedef struct hu_reldyn_state {
    char contact_id[128];
    size_t contact_id_len;
    double closeness;           /* 0.0 (stranger) to 1.0 (intimate) */
    double velocity;            /* positive = deepening, negative = drifting */
    double vulnerability_depth; /* depth of conversations: 0.0–1.0 */
    double reciprocity;         /* -1.0..1.0; 0 = balanced */
    hu_reldyn_mode_t mode;
    int64_t last_interaction_ms;
    int64_t mode_entered_ms;
    int64_t measured_at_ms;
} hu_reldyn_state_t;

typedef struct hu_reldyn_signals {
    double frequency_delta;     /* message count change vs prior window */
    double initiation_delta;    /* who starts conversations shift */
    double response_time_delta; /* response time change */
    double msg_length_delta;    /* message length change */
    double vulnerability_delta; /* sharing depth change */
    double plan_completion;     /* plan completion rate 0–1 */
    double sentiment_delta;     /* positive sentiment change */
    double topic_diversity;     /* topic diversity 0–1 */
} hu_reldyn_signals_t;

typedef struct hu_reldyn_config {
    double drift_threshold;       /* velocity below this = drifting, default -0.1 */
    double clear_drift_threshold; /* clear drift, default -0.3 */
    int repair_exit_days;         /* days of normal signals to exit repair, default 3 */
    double drift_budget_mult;     /* governor budget multiplier when drifting, default 0.5 */
} hu_reldyn_config_t;

hu_reldyn_config_t hu_reldyn_config_default(void);

hu_error_t hu_reldyn_compute(const hu_reldyn_signals_t *signals,
                              const hu_reldyn_state_t *prev,
                              const hu_reldyn_config_t *config,
                              hu_reldyn_state_t *out);

hu_error_t hu_reldyn_detect_drift(const hu_reldyn_state_t *state,
                                   const hu_reldyn_config_t *config,
                                   bool *drifting, bool *clear_drift);

hu_error_t hu_reldyn_enter_repair(hu_reldyn_state_t *state,
                                   int64_t now_ms);

hu_error_t hu_reldyn_check_repair_exit(const hu_reldyn_state_t *state,
                                        const hu_reldyn_config_t *config,
                                        int64_t now_ms, bool *should_exit);

hu_error_t hu_reldyn_build_prompt(hu_allocator_t *alloc,
                                   const hu_reldyn_state_t *state,
                                   char **out, size_t *out_len);

const char *hu_reldyn_mode_name(hu_reldyn_mode_t mode);

#endif /* HU_AGENT_RELATIONSHIP_DYNAMICS_H */
