#include "human/agent/orchestrator.h"
#include "human/agent/registry.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define HU_ORCH_WORD_MAX 64
#define HU_ORCH_WORD_SZ 48
#define HU_ORCH_CONSENSUS_DICE_MIN 0.28

static bool orch_is_word_char(unsigned char c) {
    return (bool)((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

static size_t orch_collect_words(const char *s, size_t len, char words[][HU_ORCH_WORD_SZ]) {
    size_t n = 0;
    size_t i = 0;
    while (i < len && n < HU_ORCH_WORD_MAX) {
        while (i < len && !orch_is_word_char((unsigned char)s[i]))
            i++;
        if (i >= len)
            break;
        size_t start = i;
        while (i < len && orch_is_word_char((unsigned char)s[i]))
            i++;
        size_t wlen = i - start;
        if (wlen >= HU_ORCH_WORD_SZ)
            wlen = HU_ORCH_WORD_SZ - 1;
        memcpy(words[n], s + start, wlen);
        words[n][wlen] = '\0';
        for (size_t j = 0; j < wlen; j++)
            words[n][j] = (char)tolower((unsigned char)words[n][j]);
        n++;
    }
    return n;
}

static bool orch_word_in_table(const char *w, char words[][HU_ORCH_WORD_SZ], size_t nw) {
    for (size_t i = 0; i < nw; i++) {
        if (strcmp(w, words[i]) == 0)
            return true;
    }
    return false;
}

static double orch_token_dice(const char *a, size_t alen, const char *b, size_t blen) {
    char wa[HU_ORCH_WORD_MAX][HU_ORCH_WORD_SZ];
    char wb[HU_ORCH_WORD_MAX][HU_ORCH_WORD_SZ];
    size_t na = orch_collect_words(a, alen, wa);
    size_t nb = orch_collect_words(b, blen, wb);
    if (na == 0 && nb == 0)
        return 1.0;
    if (na == 0 || nb == 0)
        return 0.0;
    size_t inter = 0;
    for (size_t i = 0; i < na; i++) {
        if (orch_word_in_table(wa[i], wb, nb))
            inter++;
    }
    return (double)(2u * inter) / (double)(na + nb);
}

static double orch_min_pairwise_dice(const hu_orchestrator_task_t *tasks, const size_t *idx,
                                     size_t nidx) {
    if (nidx < 2)
        return 1.0;
    double min_d = 1.0;
    for (size_t i = 0; i < nidx; i++) {
        for (size_t j = i + 1; j < nidx; j++) {
            const hu_orchestrator_task_t *ti = &tasks[idx[i]];
            const hu_orchestrator_task_t *tj = &tasks[idx[j]];
            double d = orch_token_dice(ti->result, ti->result_len, tj->result, tj->result_len);
            if (d < min_d)
                min_d = d;
        }
    }
    return min_d;
}

hu_error_t hu_orchestrator_create(hu_allocator_t *alloc, hu_orchestrator_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->alloc = alloc;
    out->next_task_id = 1;
    return HU_OK;
}

void hu_orchestrator_deinit(hu_orchestrator_t *orch) {
    (void)orch;
}

hu_error_t hu_orchestrator_register_agent(hu_orchestrator_t *orch, const char *agent_id,
                                         size_t id_len, const char *role, size_t role_len,
                                         const char *skills, size_t skills_len) {
    if (!orch || !agent_id)
        return HU_ERR_INVALID_ARGUMENT;
    if (orch->agent_count >= HU_ORCHESTRATOR_MAX_AGENTS)
        return HU_ERR_SUBAGENT_TOO_MANY;

    hu_agent_capability_t *a = &orch->agents[orch->agent_count];
    memset(a, 0, sizeof(*a));

    size_t n = id_len < sizeof(a->agent_id) - 1 ? id_len : sizeof(a->agent_id) - 1;
    strncpy(a->agent_id, agent_id, n);
    a->agent_id[n] = '\0';
    a->agent_id_len = n;

    if (role && role_len > 0) {
        n = role_len < sizeof(a->role) - 1 ? role_len : sizeof(a->role) - 1;
        strncpy(a->role, role, n);
        a->role[n] = '\0';
        a->role_len = n;
    }

    if (skills && skills_len > 0) {
        n = skills_len < sizeof(a->skills) - 1 ? skills_len : sizeof(a->skills) - 1;
        strncpy(a->skills, skills, n);
        a->skills[n] = '\0';
        a->skills_len = n;
    }

    a->capacity = 1.0;
    orch->agent_count++;
    return HU_OK;
}

hu_error_t hu_orchestrator_propose_split(hu_orchestrator_t *orch, const char *goal,
                                         size_t goal_len, const char **subtasks,
                                         const size_t *subtask_lens, size_t subtask_count) {
    (void)goal;
    (void)goal_len;
    if (!orch || !subtasks)
        return HU_ERR_INVALID_ARGUMENT;
    if (orch->task_count + subtask_count > HU_ORCHESTRATOR_MAX_TASKS)
        return HU_ERR_SUBAGENT_TOO_MANY;

    for (size_t i = 0; i < subtask_count; i++) {
        hu_orchestrator_task_t *t = &orch->tasks[orch->task_count];
        memset(t, 0, sizeof(*t));
        t->id = orch->next_task_id++;
        t->status = HU_TASK_UNASSIGNED;
        t->depends_on = 0;
        t->priority = 1.0;

        const char *desc = subtasks[i];
        size_t desc_len = subtask_lens ? subtask_lens[i] : (desc ? strlen(desc) : 0);
        if (desc && desc_len > 0) {
            size_t n = desc_len < sizeof(t->description) - 1 ? desc_len : sizeof(t->description) - 1;
            strncpy(t->description, desc, n);
            t->description[n] = '\0';
            t->description_len = n;
        }
        orch->task_count++;
    }
    return HU_OK;
}

hu_error_t hu_orchestrator_assign_task(hu_orchestrator_t *orch, uint32_t task_id,
                                       const char *agent_id, size_t agent_id_len) {
    if (!orch || !agent_id)
        return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < orch->task_count; i++) {
        hu_orchestrator_task_t *t = &orch->tasks[i];
        if (t->id == task_id) {
            size_t n =
                agent_id_len < sizeof(t->assigned_agent) - 1 ? agent_id_len : sizeof(t->assigned_agent) - 1;
            strncpy(t->assigned_agent, agent_id, n);
            t->assigned_agent[n] = '\0';
            t->assigned_agent_len = n;
            t->status = HU_TASK_ASSIGNED;
            return HU_OK;
        }
    }
    return HU_ERR_NOT_FOUND;
}

hu_error_t hu_orchestrator_complete_task(hu_orchestrator_t *orch, uint32_t task_id,
                                         const char *result, size_t result_len) {
    if (!orch)
        return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < orch->task_count; i++) {
        hu_orchestrator_task_t *t = &orch->tasks[i];
        if (t->id == task_id) {
            size_t n = result_len < sizeof(t->result) - 1 ? result_len : sizeof(t->result) - 1;
            if (result && result_len > 0) {
                strncpy(t->result, result, n);
                t->result[n] = '\0';
            } else {
                t->result[0] = '\0';
            }
            t->result_len = n;
            t->status = HU_TASK_COMPLETED;
            return HU_OK;
        }
    }
    return HU_ERR_NOT_FOUND;
}

hu_error_t hu_orchestrator_fail_task(hu_orchestrator_t *orch, uint32_t task_id,
                                     const char *reason, size_t reason_len) {
    if (!orch)
        return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < orch->task_count; i++) {
        hu_orchestrator_task_t *t = &orch->tasks[i];
        if (t->id == task_id) {
            size_t n = reason_len < sizeof(t->result) - 1 ? reason_len : sizeof(t->result) - 1;
            if (reason && reason_len > 0) {
                strncpy(t->result, reason, n);
                t->result[n] = '\0';
            } else {
                t->result[0] = '\0';
            }
            t->result_len = n;
            t->status = HU_TASK_FAILED;
            return HU_OK;
        }
    }
    return HU_ERR_NOT_FOUND;
}

hu_error_t hu_orchestrator_merge_results(hu_orchestrator_t *orch, hu_allocator_t *alloc,
                                         char **out, size_t *out_len) {
    if (!orch || !alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_len = 0;

    size_t total = 0;
    for (size_t i = 0; i < orch->task_count; i++) {
        if (orch->tasks[i].status == HU_TASK_COMPLETED)
            total += 32 + orch->tasks[i].result_len;
    }
    if (total == 0)
        return HU_OK;

    char *buf = (char *)alloc->alloc(alloc->ctx, total + 1);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;

    size_t pos = 0;
    for (size_t i = 0; i < orch->task_count; i++) {
        const hu_orchestrator_task_t *t = &orch->tasks[i];
        if (t->status != HU_TASK_COMPLETED)
            continue;
        int n = snprintf(buf + pos, total + 1 - pos, "Task %u: %.*s\n",
                         (unsigned)t->id, (int)t->result_len, t->result);
        if (n > 0 && pos + (size_t)n <= total)
            pos += (size_t)n;
    }
    buf[pos] = '\0';
    *out = buf;
    *out_len = pos;
    return HU_OK;
}

hu_error_t hu_orchestrator_merge_results_consensus(hu_orchestrator_t *orch, hu_allocator_t *alloc,
                                                   char **out, size_t *out_len) {
    if (!orch || !alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_len = 0;

    size_t idx[HU_ORCHESTRATOR_MAX_TASKS];
    size_t nidx = 0;
    for (size_t i = 0; i < orch->task_count && nidx < HU_ORCHESTRATOR_MAX_TASKS; i++) {
        if (orch->tasks[i].status == HU_TASK_COMPLETED)
            idx[nidx++] = i;
    }
    if (nidx == 0)
        return HU_OK;

    if (nidx == 1) {
        const hu_orchestrator_task_t *t = &orch->tasks[idx[0]];
        size_t cap = t->result_len + 1;
        char *buf = (char *)alloc->alloc(alloc->ctx, cap);
        if (!buf)
            return HU_ERR_OUT_OF_MEMORY;
        if (t->result_len > 0)
            memcpy(buf, t->result, t->result_len);
        buf[t->result_len] = '\0';
        *out = buf;
        *out_len = t->result_len;
        return HU_OK;
    }

    double min_dice = orch_min_pairwise_dice(orch->tasks, idx, nidx);
    bool consensus = min_dice >= HU_ORCH_CONSENSUS_DICE_MIN;

    if (consensus) {
        size_t win_i = 0;
        size_t best_len = 0;
        for (size_t k = 0; k < nidx; k++) {
            const hu_orchestrator_task_t *t = &orch->tasks[idx[k]];
            if (t->result_len >= best_len) {
                best_len = t->result_len;
                win_i = k;
            }
        }
        const hu_orchestrator_task_t *w = &orch->tasks[idx[win_i]];
        size_t cap = w->result_len + 1;
        char *buf = (char *)alloc->alloc(alloc->ctx, cap);
        if (!buf)
            return HU_ERR_OUT_OF_MEMORY;
        if (w->result_len > 0)
            memcpy(buf, w->result, w->result_len);
        buf[w->result_len] = '\0';
        *out = buf;
        *out_len = w->result_len;
        return HU_OK;
    }

    static const char diverge_note[] =
        "Multiple perspectives (sub-agents diverged):\n";
    size_t note_len = sizeof(diverge_note) - 1;
    size_t total = note_len;
    for (size_t k = 0; k < nidx; k++) {
        const hu_orchestrator_task_t *t = &orch->tasks[idx[k]];
        total += 32 + t->result_len;
    }

    char *buf = (char *)alloc->alloc(alloc->ctx, total + 1);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;

    memcpy(buf, diverge_note, note_len);
    size_t pos = note_len;
    for (size_t k = 0; k < nidx; k++) {
        const hu_orchestrator_task_t *t = &orch->tasks[idx[k]];
        int n = snprintf(buf + pos, total + 1 - pos, "Task %u: %.*s\n", (unsigned)t->id,
                         (int)t->result_len, t->result);
        if (n > 0 && pos + (size_t)n <= total)
            pos += (size_t)n;
    }
    buf[pos] = '\0';
    *out = buf;
    *out_len = pos;
    return HU_OK;
}

bool hu_orchestrator_all_complete(const hu_orchestrator_t *orch) {
    if (!orch || orch->task_count == 0)
        return true;
    for (size_t i = 0; i < orch->task_count; i++) {
        if (orch->tasks[i].status != HU_TASK_COMPLETED)
            return false;
    }
    return true;
}

size_t hu_orchestrator_count_by_status(const hu_orchestrator_t *orch, hu_task_status_t status) {
    if (!orch)
        return 0;
    size_t count = 0;
    for (size_t i = 0; i < orch->task_count; i++) {
        if (orch->tasks[i].status == status)
            count++;
    }
    return count;
}

static bool task_dependency_satisfied(const hu_orchestrator_t *orch, uint32_t depends_on) {
    if (depends_on == 0)
        return true;
    for (size_t i = 0; i < orch->task_count; i++) {
        if (orch->tasks[i].id == depends_on)
            return orch->tasks[i].status == HU_TASK_COMPLETED;
    }
    return true;
}

hu_error_t hu_orchestrator_next_task(hu_orchestrator_t *orch, hu_orchestrator_task_t **out) {
    if (!orch || !out)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    for (size_t i = 0; i < orch->task_count; i++) {
        hu_orchestrator_task_t *t = &orch->tasks[i];
        if (t->status == HU_TASK_UNASSIGNED && task_dependency_satisfied(orch, t->depends_on)) {
            *out = t;
            return HU_OK;
        }
    }
    return HU_ERR_NOT_FOUND;
}

const char *hu_task_status_str(hu_task_status_t status) {
    switch (status) {
    case HU_TASK_UNASSIGNED:
        return "unassigned";
    case HU_TASK_ASSIGNED:
        return "assigned";
    case HU_TASK_IN_PROGRESS:
        return "in_progress";
    case HU_TASK_COMPLETED:
        return "completed";
    case HU_TASK_FAILED:
        return "failed";
    default:
        return "unknown";
    }
}

hu_error_t hu_orchestrator_load_from_registry(hu_orchestrator_t *orch,
                                               const hu_agent_registry_t *registry) {
    if (!orch || !registry)
        return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < registry->count; i++) {
        const hu_named_agent_config_t *cfg = &registry->agents[i];
        if (!cfg->name)
            continue;

        const char *role = cfg->role ? cfg->role : "general";
        const char *caps = cfg->capabilities ? cfg->capabilities : "";

        hu_error_t err = hu_orchestrator_register_agent(
            orch, cfg->name, strlen(cfg->name),
            role, strlen(role), caps, strlen(caps));
        if (err == HU_ERR_SUBAGENT_TOO_MANY)
            break;
        if (err != HU_OK && err != HU_ERR_SUBAGENT_TOO_MANY)
            return err;
    }
    return HU_OK;
}
