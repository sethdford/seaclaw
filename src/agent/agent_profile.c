#include "human/agent/agent_profile.h"
#include <string.h>

static int category_index(const char *cat, size_t len) {
    if (len == 6 && memcmp(cat, "coding", 6) == 0)
        return 0;
    if (len == 9 && memcmp(cat, "reasoning", 9) == 0)
        return 1;
    if (len == 8 && memcmp(cat, "research", 8) == 0)
        return 2;
    if (len == 7 && memcmp(cat, "general", 7) == 0)
        return 3;
    if (len == 3 && memcmp(cat, "ops", 3) == 0)
        return 4;
    if (len == 10 && memcmp(cat, "messaging", 10) == 0)
        return 5;
    if (len == 8 && memcmp(cat, "analysis", 8) == 0)
        return 6;
    if (len == 8 && memcmp(cat, "creative", 8) == 0)
        return 7;
    return -1;
}

static int specialization_index(const char *spec, size_t len) {
    return category_index(spec, len);
}

hu_error_t hu_agent_match_score(const hu_agent_profile_t *profile,
                                const char *task_category, size_t cat_len,
                                double *score) {
    if (!profile || !score)
        return HU_ERR_INVALID_ARGUMENT;
    if (cat_len > 0 && !task_category)
        return HU_ERR_INVALID_ARGUMENT;

    int idx = category_index(task_category, cat_len);
    if (idx >= 0 && idx < HU_AGENT_PROFILE_CATEGORIES) {
        *score = profile->success_rates[idx];
        if (*score <= 0.0)
            *score = 0.5; /* default when no history */
        return HU_OK;
    }

    /* No match: use generalist (index 3) or specialization match */
    int spec_idx = specialization_index(profile->specialization,
                                        strlen(profile->specialization));
    if (spec_idx >= 0 && spec_idx < HU_AGENT_PROFILE_CATEGORIES) {
        *score = profile->success_rates[spec_idx];
    } else {
        *score = profile->success_rates[3]; /* general */
    }
    if (*score <= 0.0)
        *score = 0.5;
    return HU_OK;
}

hu_error_t hu_agent_profile_update(hu_agent_profile_t *profile,
                                    const char *task_category, size_t cat_len,
                                    bool success) {
    if (!profile)
        return HU_ERR_INVALID_ARGUMENT;

    int idx = category_index(task_category, cat_len);
    if (idx < 0 || idx >= HU_AGENT_PROFILE_CATEGORIES)
        idx = 3; /* general */

    double current = profile->success_rates[idx];
    if (current <= 0.0)
        current = 0.5;

    /* Exponential moving average with alpha=0.1 */
    double outcome = success ? 1.0 : 0.0;
    profile->success_rates[idx] = current * 0.9 + outcome * 0.1;
    profile->task_count++;
    return HU_OK;
}
