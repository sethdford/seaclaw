/*
 * Circadian persona overlay — time-of-day adaptive guidance.
 */
#include "human/persona/circadian.h"
#include "human/core/string.h"
#include "human/data/loader.h"
#include "human/core/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default fallback values */
static const char *DEFAULT_PHASE_NAMES[] = {
    "early morning", "morning", "afternoon", "evening", "night", "late night",
};

static const char *DEFAULT_PHASE_GUIDANCE[] = {
    "Be gentle and warm. The user is starting their day. Keep responses calm and encouraging.",
    "Be energetic and productive. The user is at peak mental clarity. Be direct and efficient.",
    "Be steady and focused. Energy may be dipping. Keep things clear and structured.",
    "Be relaxed and reflective. The day is winding down. Allow for deeper conversation.",
    "Be calm and intimate. The user is in a quieter headspace. Slow your pace, be thoughtful.",
    ("Be present and unhurried. Late night conversations often carry more weight. Be a quiet "
     "companion."),
};

/* Runtime loaded data */
static const char **s_phase_names = (const char **)DEFAULT_PHASE_NAMES;
static const char **s_phase_guidance = (const char **)DEFAULT_PHASE_GUIDANCE;
static size_t s_phase_count = 6;

hu_error_t hu_circadian_data_init(hu_allocator_t *alloc) {
    if (!alloc)
        return HU_ERR_INVALID_ARGUMENT;

    char *json_data = NULL;
    size_t json_len = 0;
    hu_error_t err = hu_data_load(alloc, "persona/circadian_phases.json", &json_data, &json_len);
    if (err != HU_OK)
        return HU_OK; /* Fail gracefully, keep defaults */

    hu_json_value_t *root = NULL;
    err = hu_json_parse(alloc, json_data, json_len, &root);
    alloc->free(alloc->ctx, json_data, json_len);
    if (err != HU_OK || !root)
        return HU_OK; /* Fail gracefully, keep defaults */

    hu_json_value_t *phases_arr = hu_json_object_get(root, "phases");
    if (!phases_arr || phases_arr->type != HU_JSON_ARRAY) {
        hu_json_free(alloc, root);
        return HU_OK;
    }

    size_t phase_count = phases_arr->data.array.len;
    if (phase_count == 0 || phase_count > 6) {
        hu_json_free(alloc, root);
        return HU_OK;
    }

    const char **names = (const char **)alloc->alloc(alloc->ctx, phase_count * sizeof(const char *));
    const char **guidance = (const char **)alloc->alloc(alloc->ctx, phase_count * sizeof(const char *));
    if (!names || !guidance) {
        if (names)
            alloc->free(alloc->ctx, names, phase_count * sizeof(const char *));
        if (guidance)
            alloc->free(alloc->ctx, guidance, phase_count * sizeof(const char *));
        hu_json_free(alloc, root);
        return HU_OK;
    }

    memset(names, 0, phase_count * sizeof(const char *));
    memset(guidance, 0, phase_count * sizeof(const char *));

    for (size_t i = 0; i < phase_count; i++) {
        hu_json_value_t *phase_obj = phases_arr->data.array.items[i];
        if (!phase_obj || phase_obj->type != HU_JSON_OBJECT)
            continue;

        const char *name = hu_json_get_string(phase_obj, "name");
        const char *guide = hu_json_get_string(phase_obj, "guidance");

        if (name)
            names[i] = hu_strndup(alloc, name, strlen(name));
        if (guide)
            guidance[i] = hu_strndup(alloc, guide, strlen(guide));
    }

    /* Atomically swap in new data */
    s_phase_names = names;
    s_phase_guidance = guidance;
    s_phase_count = phase_count;

    hu_json_free(alloc, root);
    return HU_OK;
}

void hu_circadian_data_cleanup(hu_allocator_t *alloc) {
    if (!alloc)
        return;
    /* Only free if they're not the defaults */
    if (s_phase_names != (const char **)DEFAULT_PHASE_NAMES) {
        for (size_t i = 0; i < s_phase_count; i++) {
            if (s_phase_names[i])
                alloc->free(alloc->ctx, (char *)s_phase_names[i], strlen(s_phase_names[i]) + 1);
        }
        alloc->free(alloc->ctx, s_phase_names, s_phase_count * sizeof(const char *));
    }
    if (s_phase_guidance != (const char **)DEFAULT_PHASE_GUIDANCE) {
        for (size_t i = 0; i < s_phase_count; i++) {
            if (s_phase_guidance[i])
                alloc->free(alloc->ctx, (char *)s_phase_guidance[i], strlen(s_phase_guidance[i]) + 1);
        }
        alloc->free(alloc->ctx, s_phase_guidance, s_phase_count * sizeof(const char *));
    }
    s_phase_names = (const char **)DEFAULT_PHASE_NAMES;
    s_phase_guidance = (const char **)DEFAULT_PHASE_GUIDANCE;
    s_phase_count = 6;
}

hu_time_phase_t hu_circadian_phase(uint8_t hour) {
    if (hour < 5)
        return HU_PHASE_LATE_NIGHT;
    if (hour < 8)
        return HU_PHASE_EARLY_MORNING;
    if (hour < 12)
        return HU_PHASE_MORNING;
    if (hour < 17)
        return HU_PHASE_AFTERNOON;
    if (hour < 21)
        return HU_PHASE_EVENING;
    /* 21:00-23:59 */
    return HU_PHASE_NIGHT;
}

hu_error_t hu_circadian_build_prompt(hu_allocator_t *alloc, uint8_t hour, char **out,
                                     size_t *out_len) {
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    hu_time_phase_t phase = hu_circadian_phase(hour);
    const char *name = s_phase_names[(size_t)phase];
    const char *guidance = s_phase_guidance[(size_t)phase];

#define HU_CIRCADIAN_BUF_CAP 256
    char *buf = (char *)alloc->alloc(alloc->ctx, HU_CIRCADIAN_BUF_CAP);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;

    int n = snprintf(buf, HU_CIRCADIAN_BUF_CAP, "\n### Time Awareness\nCurrent phase: %s. %s\n",
                     name, guidance);
    if (n <= 0 || (size_t)n >= HU_CIRCADIAN_BUF_CAP) {
        alloc->free(alloc->ctx, buf, HU_CIRCADIAN_BUF_CAP);
        return HU_ERR_INVALID_ARGUMENT;
    }

    size_t need = (size_t)n + 1;
    char *shrunk = (char *)alloc->realloc(alloc->ctx, buf, HU_CIRCADIAN_BUF_CAP, need);
    if (!shrunk) {
        alloc->free(alloc->ctx, buf, HU_CIRCADIAN_BUF_CAP);
        return HU_ERR_OUT_OF_MEMORY;
    }
    *out = shrunk;
    *out_len = (size_t)n;
#undef HU_CIRCADIAN_BUF_CAP
    return HU_OK;
}
