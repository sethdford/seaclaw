#include "human/memory/policy.h"
#include <stdio.h>
#include <string.h>

void hu_mem_policy_init(hu_mem_policy_t *p) {
    if (!p)
        return;
    memset(p, 0, sizeof(*p));
    p->recency_weight = 0.4;
    p->relevance_weight = 0.4;
    p->frequency_weight = 0.2;
}

const char *hu_mem_action_type_name(hu_mem_action_type_t type) {
    switch (type) {
    case HU_MEM_STORE:
        return "store";
    case HU_MEM_RETRIEVE:
        return "retrieve";
    case HU_MEM_UPDATE:
        return "update";
    case HU_MEM_SUMMARIZE:
        return "summarize";
    case HU_MEM_DISCARD:
        return "discard";
    default:
        return "unknown";
    }
}

double hu_mem_policy_score(const hu_mem_policy_t *p, double recency, double relevance,
                           double frequency) {
    if (!p)
        return 0.0;
    return p->recency_weight * recency + p->relevance_weight * relevance +
           p->frequency_weight * frequency;
}

hu_mem_action_type_t hu_mem_policy_decide(const hu_mem_policy_t *p, const hu_mem_state_t *state,
                                          const char *content, size_t content_len) {
    if (!p || !state)
        return HU_MEM_STORE;

    if (!content || content_len == 0)
        return HU_MEM_RETRIEVE;

    if (state->context_pressure > 0.9 && state->ltm_entries > 100)
        return HU_MEM_DISCARD;

    if (state->context_pressure > 0.8)
        return HU_MEM_SUMMARIZE;

    if (state->stm_entries > 20 && state->context_pressure > 0.5)
        return HU_MEM_SUMMARIZE;

    if (content_len > 200 && state->total_memories < 1000)
        return HU_MEM_STORE;

    if (state->avg_relevance < 0.3)
        return HU_MEM_RETRIEVE;

    return HU_MEM_STORE;
}

hu_error_t hu_mem_policy_record(hu_mem_policy_t *p, const hu_mem_experience_t *exp) {
    if (!p || !exp)
        return HU_ERR_INVALID_ARGUMENT;

    if (p->buffer_count < HU_MEM_EXPERIENCE_BUFFER_SIZE) {
        p->buffer[p->buffer_count++] = *exp;
    } else {
        size_t idx = p->total_actions % HU_MEM_EXPERIENCE_BUFFER_SIZE;
        p->buffer[idx] = *exp;
    }

    p->total_actions++;
    switch (exp->action.type) {
    case HU_MEM_STORE:
        p->store_count++;
        break;
    case HU_MEM_RETRIEVE:
        p->retrieve_count++;
        break;
    case HU_MEM_DISCARD:
        p->discard_count++;
        break;
    default:
        break;
    }

    return HU_OK;
}

size_t hu_mem_policy_report(const hu_mem_policy_t *p, char *buf, size_t buf_size) {
    if (!p || !buf || buf_size == 0)
        return 0;
    int n = snprintf(buf, buf_size,
                     "Memory Policy Report\n"
                     "====================\n"
                     "Total actions: %zu\n"
                     "  Store: %zu\n"
                     "  Retrieve: %zu\n"
                     "  Discard: %zu\n"
                     "Experience buffer: %zu / %d\n"
                     "Weights: recency=%.2f relevance=%.2f frequency=%.2f\n",
                     p->total_actions, p->store_count, p->retrieve_count, p->discard_count,
                     p->buffer_count, HU_MEM_EXPERIENCE_BUFFER_SIZE, p->recency_weight,
                     p->relevance_weight, p->frequency_weight);
    if (n < 0)
        return 0;
    return (size_t)n < buf_size ? (size_t)n : buf_size - 1;
}
