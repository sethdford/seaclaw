#include "human/cognition/presence.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

/* Shared pattern: also in attachment.c */
static float clamp01(float x) {
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

void hu_presence_init(hu_presence_state_t *state) {
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
    state->level = HU_PRESENCE_ENGAGED;
    state->attention_score = 0.5f;
    state->memory_budget_multiplier = 1u;
    state->upgrade_model_tier = false;
    state->presence_directive = NULL;
}

void hu_presence_deinit(hu_allocator_t *alloc, hu_presence_state_t *state) {
    if (!alloc || !state)
        return;
    hu_str_free(alloc, state->presence_directive);
    state->presence_directive = NULL;
}

const char *hu_presence_level_name(hu_presence_level_t level) {
    switch (level) {
    case HU_PRESENCE_CASUAL:
        return "casual";
    case HU_PRESENCE_ENGAGED:
        return "engaged";
    case HU_PRESENCE_ATTENTIVE:
        return "attentive";
    case HU_PRESENCE_DEEP:
        return "deep";
    default:
        return "unknown";
    }
}

void hu_presence_compute(hu_presence_state_t *state,
                         float emotional_weight, float vulnerability_signal,
                         float topic_sensitivity, float user_affect_intensity,
                         uint32_t relationship_depth) {
    if (!state)
        return;

    float depth_term = (float)relationship_depth / 10.0f;
    if (depth_term > 1.0f)
        depth_term = 1.0f;

    float score = 0.3f * clamp01(emotional_weight) + 0.25f * clamp01(vulnerability_signal) +
                  0.2f * clamp01(topic_sensitivity) + 0.15f * clamp01(user_affect_intensity) +
                  0.1f * depth_term;
    state->attention_score = clamp01(score);

    if (state->attention_score < 0.25f) {
        state->level = HU_PRESENCE_CASUAL;
        state->memory_budget_multiplier = 1u;
    } else if (state->attention_score < 0.5f) {
        state->level = HU_PRESENCE_ENGAGED;
        state->memory_budget_multiplier = 1u;
    } else if (state->attention_score < 0.75f) {
        state->level = HU_PRESENCE_ATTENTIVE;
        state->memory_budget_multiplier = 2u;
    } else {
        state->level = HU_PRESENCE_DEEP;
        state->memory_budget_multiplier = 3u;
    }
    state->upgrade_model_tier = (state->level == HU_PRESENCE_DEEP);
}

hu_error_t hu_presence_build_context(hu_allocator_t *alloc, const hu_presence_state_t *state,
                                     char **out, size_t *out_len) {
    if (!alloc || !state || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    const char *text;
    switch (state->level) {
    case HU_PRESENCE_CASUAL:
        text = "[PRESENCE: Light mode. Brief, breezy. Don't over-invest.]";
        break;
    case HU_PRESENCE_ENGAGED:
        text = "[PRESENCE: Standard attention. Be present and responsive.]";
        break;
    case HU_PRESENCE_ATTENTIVE:
        text = "[PRESENCE: This matters to them. Draw on what you know. Ask thoughtful questions.]";
        break;
    case HU_PRESENCE_DEEP:
        text = "[PRESENCE: Deep attention. This matters. Take your time. Draw on everything you "
               "know about them. Ask the question that shows you really heard them.]";
        break;
    default:
        text = "[PRESENCE: Standard attention. Be present and responsive.]";
        break;
    }

    size_t tl = strlen(text);
    char *copy = hu_strndup(alloc, text, tl);
    if (!copy)
        return HU_ERR_OUT_OF_MEMORY;
    *out = copy;
    *out_len = tl;
    return HU_OK;
}
