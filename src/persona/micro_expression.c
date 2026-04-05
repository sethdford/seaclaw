#include "human/persona/micro_expression.h"
#include "human/core/string.h"
#include <math.h>
#include <string.h>

void hu_micro_expression_init(hu_micro_expression_t *me) {
    if (!me)
        return;
    me->target_length_factor = 1.0f;
    me->punctuation_density = 0.5f;
    me->emoji_probability = 0.1f;
    me->capitalization_energy = 1.0f;
    me->ellipsis_frequency = 0.1f;
    me->use_fragments = false;
    me->style_directive = NULL;
}

void hu_micro_expression_deinit(hu_allocator_t *alloc, hu_micro_expression_t *me) {
    if (!alloc || !me)
        return;
    hu_str_free(alloc, me->style_directive);
    me->style_directive = NULL;
}

void hu_micro_expression_compute(hu_micro_expression_t *me,
                                 float energy, float social_battery,
                                 float emotional_valence, float emotional_intensity,
                                 float relationship_closeness) {
    if (!me)
        return;

    me->target_length_factor = 1.0f;
    me->punctuation_density = 0.5f;
    me->emoji_probability = 0.1f;
    me->capitalization_energy = 1.0f;
    me->ellipsis_frequency = 0.1f;
    me->use_fragments = false;

    if (energy < 0.3f) {
        me->target_length_factor = 0.6f;
        me->ellipsis_frequency += 0.3f;
        me->capitalization_energy -= 0.3f;
        me->use_fragments = true;
    } else if (energy > 0.8f) {
        me->target_length_factor = 1.3f;
        me->punctuation_density += 0.2f;
    }

    if (social_battery < 0.3f) {
        me->target_length_factor *= 0.7f;
        me->emoji_probability -= 0.05f;
    }

    const float hi = 0.6f;
    const float lo = 0.35f;

    if (emotional_valence > 0.f && emotional_intensity > hi) {
        me->punctuation_density += 0.3f;
        me->emoji_probability += 0.15f;
        me->target_length_factor *= 1.1f;
    } else if (emotional_valence < 0.f && emotional_intensity > hi) {
        me->emoji_probability = 0.f;
        me->ellipsis_frequency += 0.2f;
        me->use_fragments = true;
        me->target_length_factor *= 0.8f;
    } else if (emotional_valence < 0.f && emotional_intensity < lo) {
        me->ellipsis_frequency += 0.4f;
        me->capitalization_energy -= 0.2f;
        me->target_length_factor *= 0.5f;
    }

    if (relationship_closeness > 0.7f) {
        const float c_tlf = 1.0f;
        const float c_p = 0.5f;
        const float c_e = 0.1f;
        const float c_cap = 1.0f;
        const float c_el = 0.1f;
        me->target_length_factor = c_tlf + (me->target_length_factor - c_tlf) * 1.3f;
        me->punctuation_density = c_p + (me->punctuation_density - c_p) * 1.3f;
        me->emoji_probability = c_e + (me->emoji_probability - c_e) * 1.3f;
        me->capitalization_energy = c_cap + (me->capitalization_energy - c_cap) * 1.3f;
        me->ellipsis_frequency = c_el + (me->ellipsis_frequency - c_el) * 1.3f;
    }

    me->target_length_factor = fmaxf(0.f, fminf(2.f, me->target_length_factor));
    me->punctuation_density = fmaxf(0.f, fminf(1.f, me->punctuation_density));
    me->emoji_probability = fmaxf(0.f, fminf(1.f, me->emoji_probability));
    me->capitalization_energy = fmaxf(0.f, fminf(1.f, me->capitalization_energy));
    me->ellipsis_frequency = fmaxf(0.f, fminf(1.f, me->ellipsis_frequency));
}

static const char *micro_pick_directive(const hu_micro_expression_t *me) {
    if (!me)
        return NULL;
    if (me->ellipsis_frequency >= 0.48f && me->target_length_factor <= 0.55f)
        return "Write shorter. Fragments are okay. Minimal emoji. Let thoughts trail with ellipses...";
    if (me->target_length_factor >= 1.08f && me->punctuation_density >= 0.68f &&
        me->emoji_probability >= 0.18f)
        return "You're energized. A bit more punctuation, slightly longer, let enthusiasm show "
               "through naturally.";
    if (me->use_fragments && me->capitalization_energy <= 0.78f &&
        me->target_length_factor <= 0.82f)
        return "You're a bit tired. Write shorter. Use ellipses naturally. Drop some capitalization. "
               "Let sentences trail off...";
    return NULL;
}

hu_error_t hu_micro_expression_build_context(hu_allocator_t *alloc,
                                             const hu_micro_expression_t *me,
                                             char **out, size_t *out_len) {
    if (!alloc || !me || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_len = 0;

    const char *inner = micro_pick_directive(me);
    if (!inner) {
        return HU_OK;
    }

    static const char prefix[] = "[MICRO-EXPRESSION: ";
    static const char suffix[] = "]";

    size_t inner_len = strlen(inner);
    size_t total = sizeof(prefix) - 1U + inner_len + sizeof(suffix) - 1U;

    char *buf = (char *)alloc->alloc(alloc->ctx, total + 1U);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;

    memcpy(buf, prefix, sizeof(prefix) - 1U);
    memcpy(buf + sizeof(prefix) - 1U, inner, inner_len);
    memcpy(buf + sizeof(prefix) - 1U + inner_len, suffix, sizeof(suffix) - 1U);
    buf[total] = '\0';

    *out = buf;
    *out_len = total;
    return HU_OK;
}
