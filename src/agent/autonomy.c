#include "human/agent/autonomy.h"
#include <string.h>
#include <time.h>

static int64_t now_ms(void) {
    return (int64_t)time(NULL) * 1000;
}

static void sort_goals_by_priority(hu_autonomy_goal_t *goals, size_t count) {
    for (size_t i = 0; i + 1 < count; i++) {
        size_t best = i;
        for (size_t j = i + 1; j < count; j++) {
            if (goals[j].priority > goals[best].priority && !goals[j].completed)
                best = j;
        }
        if (best != i) {
            hu_autonomy_goal_t tmp = goals[i];
            goals[i] = goals[best];
            goals[best] = tmp;
        }
    }
}

hu_error_t hu_autonomy_init(hu_autonomy_state_t *state, size_t context_budget) {
    if (!state)
        return HU_ERR_INVALID_ARGUMENT;
    memset(state, 0, sizeof(*state));
    state->context_budget = context_budget > 0 ? context_budget : HU_AUTONOMY_DEFAULT_BUDGET;
    state->session_start = now_ms();
    state->last_consolidation = state->session_start;
    return HU_OK;
}

hu_error_t hu_autonomy_add_goal(hu_autonomy_state_t *state, const char *desc, size_t desc_len,
                                 double priority) {
    if (!state || !desc)
        return HU_ERR_INVALID_ARGUMENT;
    if (state->goal_count >= HU_AUTONOMY_MAX_GOALS)
        return HU_ERR_OUT_OF_MEMORY;
    hu_autonomy_goal_t *g = &state->goals[state->goal_count];
    memset(g, 0, sizeof(*g));
    size_t copy_len = desc_len > 511 ? 511 : desc_len;
    if (copy_len > 0)
        memcpy(g->description, desc, copy_len);
    g->description[copy_len] = '\0';
    g->description_len = copy_len;
    g->priority = priority;
    g->completed = false;
    g->created_at = now_ms();
    g->deadline = 0;
    state->goal_count++;
    return HU_OK;
}

hu_error_t hu_autonomy_get_next_goal(const hu_autonomy_state_t *state, hu_autonomy_goal_t *out) {
    if (!state || !out)
        return HU_ERR_INVALID_ARGUMENT;
    hu_autonomy_goal_t *goals = (hu_autonomy_goal_t *)state->goals;
    size_t count = state->goal_count;
    size_t best = (size_t)-1;
    double best_pri = -1.0;
    for (size_t i = 0; i < count; i++) {
        if (!goals[i].completed && goals[i].priority > best_pri) {
            best_pri = goals[i].priority;
            best = i;
        }
    }
    if (best == (size_t)-1)
        return HU_ERR_NOT_FOUND;
    *out = goals[best];
    return HU_OK;
}

hu_error_t hu_autonomy_mark_complete(hu_autonomy_state_t *state, size_t goal_index) {
    if (!state)
        return HU_ERR_INVALID_ARGUMENT;
    if (goal_index >= state->goal_count)
        return HU_ERR_INVALID_ARGUMENT;
    state->goals[goal_index].completed = true;
    return HU_OK;
}

bool hu_autonomy_needs_consolidation(const hu_autonomy_state_t *state, int64_t now_ms) {
    if (!state)
        return false;
    if (state->context_budget == 0)
        return false;
    size_t threshold = (size_t)(state->context_budget * 0.8);
    if (state->context_tokens_used > threshold)
        return true;
    if (now_ms - state->last_consolidation >= HU_AUTONOMY_CONSOLIDATION_INTERVAL_MS)
        return true;
    return false;
}

hu_error_t hu_autonomy_consolidate(hu_autonomy_state_t *state) {
    if (!state)
        return HU_ERR_INVALID_ARGUMENT;
    sort_goals_by_priority(state->goals, state->goal_count);
    for (size_t i = 0; i < state->goal_count;) {
        if (state->goals[i].completed) {
            for (size_t j = i; j + 1 < state->goal_count; j++)
                state->goals[j] = state->goals[j + 1];
            state->goal_count--;
        } else {
            i++;
        }
    }
    state->context_tokens_used = 0;
    state->last_consolidation = now_ms();
    return HU_OK;
}
