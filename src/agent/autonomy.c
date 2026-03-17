#include "human/agent/autonomy.h"
#include <stdio.h>
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

hu_error_t hu_autonomy_generate_intrinsic_goal(hu_autonomy_state_t *state,
                                                size_t completed_count, size_t failed_count) {
    if (!state)
        return HU_ERR_INVALID_ARGUMENT;
    if (state->goal_count >= HU_AUTONOMY_MAX_GOALS)
        return HU_ERR_OUT_OF_MEMORY;

    char desc[512];
    int n = 0;
    double priority = 0.5;

    if (failed_count > completed_count && failed_count > 3) {
        n = snprintf(desc, sizeof(desc),
                     "Investigate recurring failures (%zu recent) and adapt approach",
                     failed_count);
        priority = 0.9;
    } else if (completed_count > 5 && state->goal_count == 0) {
        n = snprintf(desc, sizeof(desc),
                     "Review %zu completed tasks for improvement patterns",
                     completed_count);
        priority = 0.4;
    } else if (state->goal_count == 0) {
        n = snprintf(desc, sizeof(desc),
                     "Proactively check pending schedules and maintenance tasks");
        priority = 0.3;
    } else {
        return HU_OK;
    }

    if (n <= 0)
        return HU_ERR_INTERNAL;
    return hu_autonomy_add_goal(state, desc, (size_t)n, priority);
}

hu_error_t hu_autonomy_externalize_state(const hu_autonomy_state_t *state,
                                          char *buf, size_t buf_size, size_t *out_len) {
    if (!state || !buf || !out_len || buf_size < 32)
        return HU_ERR_INVALID_ARGUMENT;

    int pos = snprintf(buf, buf_size, "{\"goals\":%zu,\"budget\":%zu,\"used\":%zu,\"items\":[",
                       state->goal_count, state->context_budget, state->context_tokens_used);
    if (pos < 0 || (size_t)pos >= buf_size)
        return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < state->goal_count && (size_t)pos < buf_size - 64; i++) {
        const hu_autonomy_goal_t *g = &state->goals[i];
        if (i > 0 && (size_t)pos < buf_size - 1)
            buf[pos++] = ',';
        int n = snprintf(buf + pos, buf_size - (size_t)pos,
                         "{\"d\":\"%.*s\",\"p\":%.2f,\"c\":%s}",
                         (int)(g->description_len < 80 ? g->description_len : 80),
                         g->description,
                         g->priority,
                         g->completed ? "true" : "false");
        if (n > 0)
            pos += n;
    }

    if ((size_t)pos < buf_size - 2) {
        buf[pos++] = ']';
        buf[pos++] = '}';
        buf[pos] = '\0';
    }
    *out_len = (size_t)pos;
    return HU_OK;
}

hu_error_t hu_autonomy_restore_state(hu_autonomy_state_t *state,
                                      const char *buf, size_t buf_len) {
    if (!state || !buf || buf_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    /* Parse goals count */
    const char *p = strstr(buf, "\"goals\":");
    if (p) {
        p += 8;
        size_t goal_count = 0;
        sscanf(p, "%zu", &goal_count);
        (void)goal_count;
    }

    /* Parse budget */
    p = strstr(buf, "\"budget\":");
    if (p) {
        p += 9;
        sscanf(p, "%zu", &state->context_budget);
    }

    /* Parse used */
    p = strstr(buf, "\"used\":");
    if (p) {
        p += 7;
        sscanf(p, "%zu", &state->context_tokens_used);
    }

    /* Parse items array: each {"d":"...","p":0.50,"c":false} */
    state->goal_count = 0;
    p = strstr(buf, "\"items\":[");
    if (p) {
        p += 9;
        const char *end = buf + buf_len;
        while (p < end && state->goal_count < HU_AUTONOMY_MAX_GOALS) {
            const char *obj = memchr(p, '{', (size_t)(end - p));
            if (!obj) break;
            const char *obj_end = memchr(obj, '}', (size_t)(end - obj));
            if (!obj_end) break;

            hu_autonomy_goal_t *g = &state->goals[state->goal_count];
            memset(g, 0, sizeof(*g));

            /* Parse description */
            const char *d = strstr(obj, "\"d\":\"");
            if (d && d < obj_end) {
                d += 5;
                const char *dq = memchr(d, '"', (size_t)(obj_end - d));
                if (dq) {
                    size_t len = (size_t)(dq - d);
                    if (len > sizeof(g->description) - 1)
                        len = sizeof(g->description) - 1;
                    memcpy(g->description, d, len);
                    g->description_len = len;
                }
            }

            /* Parse priority */
            const char *pp = strstr(obj, "\"p\":");
            if (pp && pp < obj_end) {
                pp += 4;
                sscanf(pp, "%lf", &g->priority);
            }

            /* Parse completed */
            const char *cp = strstr(obj, "\"c\":");
            if (cp && cp < obj_end) {
                cp += 4;
                while (*cp == ' ') cp++;
                g->completed = (*cp == 't');
            }

            state->goal_count++;
            p = obj_end + 1;
        }
    }

    return HU_OK;
}
