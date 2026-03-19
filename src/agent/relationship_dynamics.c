#include "human/agent/relationship_dynamics.h"
#include <stdio.h>
#include <string.h>

hu_reldyn_config_t hu_reldyn_config_default(void) {
    return (hu_reldyn_config_t){
        .drift_threshold = -0.1,
        .clear_drift_threshold = -0.3,
        .repair_exit_days = 3,
        .drift_budget_mult = 0.5,
    };
}

const char *hu_reldyn_mode_name(hu_reldyn_mode_t mode) {
    switch (mode) {
    case HU_RELDYN_NORMAL:
        return "normal";
    case HU_RELDYN_DEEPENING:
        return "deepening";
    case HU_RELDYN_DRIFTING:
        return "drifting";
    case HU_RELDYN_REPAIR:
        return "repair";
    case HU_RELDYN_RECONNECTING:
        return "reconnecting";
    }
    return "unknown";
}

hu_error_t hu_reldyn_compute(const hu_reldyn_signals_t *signals,
                              const hu_reldyn_state_t *prev,
                              const hu_reldyn_config_t *config,
                              hu_reldyn_state_t *out) {
    if (!signals || !out)
        return HU_ERR_INVALID_ARGUMENT;

    hu_reldyn_config_t cfg = config ? *config : hu_reldyn_config_default();

    if (prev)
        *out = *prev;
    else
        memset(out, 0, sizeof(*out));

    /* Weighted velocity from signal deltas */
    double velocity =
        signals->frequency_delta * 0.20 +
        signals->initiation_delta * 0.15 +
        signals->response_time_delta * 0.15 +
        signals->msg_length_delta * 0.10 +
        signals->vulnerability_delta * 0.15 +
        signals->plan_completion * 0.10 +
        signals->sentiment_delta * 0.10 +
        signals->topic_diversity * 0.05;

    out->velocity = velocity;

    /* Update closeness with EMA */
    double prev_closeness = prev ? prev->closeness : 0.5;
    out->closeness = prev_closeness + velocity * 0.1;
    if (out->closeness > 1.0) out->closeness = 1.0;
    if (out->closeness < 0.0) out->closeness = 0.0;

    /* Update vulnerability depth */
    if (signals->vulnerability_delta > 0)
        out->vulnerability_depth += signals->vulnerability_delta * 0.1;
    if (out->vulnerability_depth > 1.0) out->vulnerability_depth = 1.0;
    if (out->vulnerability_depth < 0.0) out->vulnerability_depth = 0.0;

    /* Reciprocity tracks initiation balance */
    out->reciprocity = signals->initiation_delta;
    if (out->reciprocity > 1.0) out->reciprocity = 1.0;
    if (out->reciprocity < -1.0) out->reciprocity = -1.0;

    /* Mode detection (don't override repair mode here) */
    if (out->mode != HU_RELDYN_REPAIR) {
        if (velocity > 0.15) {
            out->mode = HU_RELDYN_DEEPENING;
        } else if (velocity < cfg.clear_drift_threshold) {
            out->mode = HU_RELDYN_DRIFTING;
        } else if (velocity < cfg.drift_threshold) {
            out->mode = HU_RELDYN_DRIFTING;
        } else if (prev && prev->mode == HU_RELDYN_DRIFTING && velocity >= 0.0) {
            out->mode = HU_RELDYN_RECONNECTING;
        } else {
            out->mode = HU_RELDYN_NORMAL;
        }
    }

    return HU_OK;
}

hu_error_t hu_reldyn_detect_drift(const hu_reldyn_state_t *state,
                                   const hu_reldyn_config_t *config,
                                   bool *drifting, bool *clear_drift) {
    if (!state || !drifting || !clear_drift)
        return HU_ERR_INVALID_ARGUMENT;

    hu_reldyn_config_t cfg = config ? *config : hu_reldyn_config_default();

    *drifting = (state->velocity < cfg.drift_threshold);
    *clear_drift = (state->velocity < cfg.clear_drift_threshold);
    return HU_OK;
}

hu_error_t hu_reldyn_enter_repair(hu_reldyn_state_t *state, int64_t now_ms) {
    if (!state)
        return HU_ERR_INVALID_ARGUMENT;
    state->mode = HU_RELDYN_REPAIR;
    state->mode_entered_ms = now_ms;
    return HU_OK;
}

hu_error_t hu_reldyn_check_repair_exit(const hu_reldyn_state_t *state,
                                        const hu_reldyn_config_t *config,
                                        int64_t now_ms, bool *should_exit) {
    if (!state || !should_exit)
        return HU_ERR_INVALID_ARGUMENT;
    *should_exit = false;

    if (state->mode != HU_RELDYN_REPAIR)
        return HU_OK;

    hu_reldyn_config_t cfg = config ? *config : hu_reldyn_config_default();
    int64_t repair_duration_ms = now_ms - state->mode_entered_ms;
    int64_t exit_threshold_ms = (int64_t)cfg.repair_exit_days * 24 * 60 * 60 * 1000;

    if (repair_duration_ms >= exit_threshold_ms && state->velocity >= 0.0)
        *should_exit = true;

    return HU_OK;
}

hu_error_t hu_reldyn_build_prompt(hu_allocator_t *alloc,
                                   const hu_reldyn_state_t *state,
                                   char **out, size_t *out_len) {
    if (!alloc || !state || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    char buf[512];
    int n = snprintf(buf, sizeof(buf),
        "[RELATIONSHIP DYNAMICS for %.*s]:\n"
        "- Closeness: %.2f, Velocity: %+.2f (%s)\n"
        "- Vulnerability depth: %.2f, Reciprocity: %+.2f\n"
        "- Mode: %s",
        (int)(state->contact_id_len < 32 ? state->contact_id_len : 32),
        state->contact_id,
        state->closeness, state->velocity,
        state->velocity > 0.05 ? "deepening" :
            (state->velocity < -0.05 ? "cooling" : "stable"),
        state->vulnerability_depth, state->reciprocity,
        hu_reldyn_mode_name(state->mode));

    if (n <= 0 || (size_t)n >= sizeof(buf))
        return HU_ERR_INTERNAL;

    char *result = (char *)alloc->alloc(alloc->ctx, (size_t)n + 1);
    if (!result)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(result, buf, (size_t)n + 1);
    *out = result;
    *out_len = (size_t)n;
    return HU_OK;
}
