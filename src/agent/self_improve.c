#include "human/agent/self_improve.h"
#include <stdio.h>
#include <string.h>

/* ── Fidelity score computation ──────────────────────────────────── */

float hu_fidelity_composite(const hu_fidelity_score_t *score) {
    if (!score)
        return 0.0f;
    return score->personality_consistency * 0.20f +
           score->vulnerability_willingness * 0.15f +
           score->humor_naturalness * 0.15f +
           score->opinion_having * 0.15f +
           score->energy_matching * 0.15f +
           score->genuine_warmth * 0.20f;
}

/* ── Initialization ──────────────────────────────────────────────── */

void hu_self_improve_init(hu_self_improve_state_t *state,
                          const hu_self_improve_config_t *config) {
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
    if (config)
        state->config = *config;
    else
        state->config = HU_SELF_IMPROVE_DEFAULTS;
}

void hu_self_improve_set_baseline(hu_self_improve_state_t *state,
                                  const hu_fidelity_score_t *baseline) {
    if (!state || !baseline)
        return;
    state->current_baseline = *baseline;
    state->current_baseline.composite = hu_fidelity_composite(baseline);
}

/* ── Mutation proposal ───────────────────────────────────────────── */

static const char *mutation_templates[] = {
    "Increase use of casual contractions and informal language",
    "Add more specific personal opinions when asked about preferences",
    "Reduce hedging language: replace 'I think maybe' with direct statements",
    "Match user's energy level more closely: short messages get short replies",
    "Include more self-deprecating humor when appropriate",
    "Push back gently when you disagree instead of defaulting to agreement",
    "Use more specific memories and callbacks to previous conversations",
    "Vary sentence length more: mix short punchy lines with longer thoughts",
    "Express genuine uncertainty with calibrated language, not false confidence",
    "Show warmth through specific observations about the person, not generic praise",
};
static const size_t template_count = sizeof(mutation_templates) / sizeof(mutation_templates[0]);

hu_error_t hu_self_improve_propose(hu_allocator_t *alloc,
                                   const hu_self_improve_state_t *state,
                                   char **mutation, size_t *mutation_len) {
    if (!alloc || !state || !mutation || !mutation_len)
        return HU_ERR_INVALID_ARGUMENT;

    size_t idx = state->experiments_run % template_count;
    const char *tmpl = mutation_templates[idx];
    size_t tlen = strlen(tmpl);

    char *buf = (char *)alloc->alloc(alloc->ctx, tlen + 1);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(buf, tmpl, tlen + 1);
    *mutation = buf;
    *mutation_len = tlen;
    return HU_OK;
}

/* ── Record experiment ───────────────────────────────────────────── */

bool hu_self_improve_record(hu_self_improve_state_t *state,
                            const char *mutation, size_t mutation_len,
                            const hu_fidelity_score_t *after) {
    if (!state || !after)
        return false;

    hu_fidelity_score_t scored = *after;
    scored.composite = hu_fidelity_composite(after);
    float delta = scored.composite - state->current_baseline.composite;
    bool keep = (delta >= state->config.min_improvement);

    if (state->history_count < HU_SELF_IMPROVE_MAX_HISTORY) {
        hu_experiment_t *exp = &state->history[state->history_count++];
        exp->id = state->experiments_run;
        if (mutation && mutation_len > 0) {
            size_t clen = mutation_len < HU_SELF_IMPROVE_MUTATION_MAX_LEN - 1
                              ? mutation_len
                              : HU_SELF_IMPROVE_MUTATION_MAX_LEN - 1;
            memcpy(exp->mutation, mutation, clen);
            exp->mutation[clen] = '\0';
            exp->mutation_len = clen;
        }
        exp->before = state->current_baseline;
        exp->after = scored;
        exp->delta = delta;
        exp->kept = keep;
    }

    state->experiments_run++;
    if (keep) {
        state->experiments_kept++;
        state->current_baseline = scored;
    }
    return keep;
}

/* ── Budget check ────────────────────────────────────────────────── */

bool hu_self_improve_budget_exhausted(const hu_self_improve_state_t *state) {
    if (!state)
        return true;
    return state->experiments_run >= state->config.max_experiments;
}
