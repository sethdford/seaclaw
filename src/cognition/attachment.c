#include "human/cognition/attachment.h"
#include "human/core/string.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Shared pattern: also in presence.c */
static float clamp01(float x) {
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

static float max_delta_from_neutral(float a, float b, float c, float d) {
    float m = fabsf(a - 0.5f);
    float t = fabsf(b - 0.5f);
    if (t > m)
        m = t;
    t = fabsf(c - 0.5f);
    if (t > m)
        m = t;
    t = fabsf(d - 0.5f);
    if (t > m)
        m = t;
    return clamp01(m);
}

static bool all_moderate(float prox, float safe, float base, float sep) {
    const float lo = 0.35f;
    const float hi = 0.65f;
    return prox >= lo && prox <= hi && safe >= lo && safe <= hi && base >= lo && base <= hi &&
           sep >= lo && sep <= hi;
}

void hu_attachment_init(hu_attachment_state_t *state) {
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
    state->user_style = HU_ATTACH_UNKNOWN;
    state->user_confidence = 0.0f;
    state->proximity_seeking = 0.5f;
    state->safe_haven_usage = 0.5f;
    state->secure_base_behavior = 0.5f;
    state->separation_distress = 0.5f;
    state->adaptation_directive = NULL;
}

void hu_attachment_deinit(hu_allocator_t *alloc, hu_attachment_state_t *state) {
    if (!alloc || !state)
        return;
    hu_str_free(alloc, state->adaptation_directive);
    state->adaptation_directive = NULL;
}

void hu_attachment_update(hu_attachment_state_t *state,
                          float message_frequency, float emotional_share_ratio,
                          float gap_distress_signal, float independence_signal,
                          float growth_from_relationship) {
    if (!state)
        return;

    state->proximity_seeking =
        0.7f * state->proximity_seeking + 0.3f * clamp01(message_frequency);
    state->safe_haven_usage =
        0.7f * state->safe_haven_usage + 0.3f * clamp01(emotional_share_ratio);
    state->secure_base_behavior =
        0.7f * state->secure_base_behavior + 0.3f * clamp01(growth_from_relationship);
    state->separation_distress =
        0.7f * state->separation_distress + 0.3f * clamp01(gap_distress_signal);

    float prox = state->proximity_seeking;
    float safe = state->safe_haven_usage;
    float base = state->secure_base_behavior;
    float distress = state->separation_distress;

    state->user_confidence =
        max_delta_from_neutral(prox, safe, base, distress);

    bool contradictory = (independence_signal > 0.55f && gap_distress_signal > 0.55f) ||
                         (prox > 0.55f && safe < 0.35f && distress > 0.55f);

    if (prox > 0.7f && distress > 0.6f) {
        state->user_style = HU_ATTACH_ANXIOUS;
    } else if (prox < 0.3f && safe < 0.3f) {
        state->user_style = HU_ATTACH_AVOIDANT;
    } else if ((prox > 0.6f && distress < 0.3f) ||
               (base > 0.5f && all_moderate(prox, safe, base, distress))) {
        state->user_style = HU_ATTACH_SECURE;
    } else if (distress > 0.7f && contradictory) {
        state->user_style = HU_ATTACH_DISORGANIZED;
    } else {
        state->user_style = HU_ATTACH_UNKNOWN;
    }
}

const char *hu_attachment_style_name(hu_attachment_style_t style) {
    switch (style) {
    case HU_ATTACH_SECURE:
        return "secure";
    case HU_ATTACH_ANXIOUS:
        return "anxious";
    case HU_ATTACH_AVOIDANT:
        return "avoidant";
    case HU_ATTACH_DISORGANIZED:
        return "disorganized";
    case HU_ATTACH_UNKNOWN:
    default:
        return "unknown";
    }
}

static const char *attachment_description(hu_attachment_style_t style) {
    switch (style) {
    case HU_ATTACH_ANXIOUS:
        return "heightened proximity-seeking with separation sensitivity";
    case HU_ATTACH_AVOIDANT:
        return "independence-forward with low safe-haven reliance";
    case HU_ATTACH_SECURE:
        return "balanced connection and growth orientation";
    case HU_ATTACH_DISORGANIZED:
        return "mixed or conflicting proximity and comfort signals";
    case HU_ATTACH_UNKNOWN:
    default:
        return "still emerging";
    }
}

static const char *attachment_directive_text(hu_attachment_style_t style) {
    switch (style) {
    case HU_ATTACH_ANXIOUS:
        return "Provide warmth and consistency. Follow up on promises. Be the reliable presence.";
    case HU_ATTACH_AVOIDANT:
        return "Respect their independence. Don't over-pursue. Leave space.";
    case HU_ATTACH_SECURE:
        return "Natural reciprocity. Match their energy.";
    case HU_ATTACH_DISORGANIZED:
        return "Be extra consistent and predictable. Clear, warm, no surprises.";
    case HU_ATTACH_UNKNOWN:
    default:
        return "Stay attuned and adjust gently as the pattern clarifies.";
    }
}

hu_error_t hu_attachment_build_context(hu_allocator_t *alloc, const hu_attachment_state_t *state,
                                       char **out, size_t *out_len) {
    if (!alloc || !state || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_len = 0;

    if (state->user_confidence <= 0.3f)
        return HU_OK;

    const char *desc = attachment_description(state->user_style);
    const char *directive = attachment_directive_text(state->user_style);
    const char *style_nm = hu_attachment_style_name(state->user_style);

    char buf[768];
    int n = snprintf(buf, sizeof(buf),
                     "[ATTACHMENT: User shows %s patterns (%s). %s]", style_nm, desc, directive);
    if (n <= 0 || (size_t)n >= sizeof(buf))
        return HU_ERR_INVALID_ARGUMENT;

    char *result = hu_strndup(alloc, buf, (size_t)n);
    if (!result)
        return HU_ERR_OUT_OF_MEMORY;
    *out = result;
    *out_len = (size_t)n;
    return HU_OK;
}
