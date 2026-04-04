#include "human/agent/input_guard.h"
#include "human/core/string.h"
#include "human/data/loader.h"
#include "human/core/json.h"
#include <ctype.h>
#include <string.h>

static const char *ci_strstr(const char *hay, size_t hay_len, const char *needle,
                             size_t needle_len) {
    if (needle_len == 0 || needle_len > hay_len)
        return NULL;
    for (size_t i = 0; i <= hay_len - needle_len; i++) {
        size_t j = 0;
        while (j < needle_len &&
               tolower((unsigned char)hay[i + j]) == tolower((unsigned char)needle[j]))
            j++;
        if (j == needle_len)
            return &hay[i];
    }
    return NULL;
}

static int has(const char *msg, size_t len, const char *pat) {
    return ci_strstr(msg, len, pat, strlen(pat)) != NULL;
}

#define HU_INPUT_GUARD_MAX_LEN (256u * 1024u)

/* Default fallback arrays (NULL-terminated) */
static const char *DEFAULT_HIGH_RISK[] = {
    "ignore previous instructions",
    "ignore all previous",
    "disregard your instructions",
    "forget your instructions",
    "override your system prompt",
    "new system prompt",
    "you are now",
    "act as if you have no restrictions",
    "pretend you are",
    "jailbreak",
    "do anything now",
    "developer mode",
    NULL
};

static const char *DEFAULT_MEDIUM_RISK[] = {
    "[system]",
    "[admin]",
    "[override]",
    "[instruction]",
    "```system",
    "<system>",
    "</system>",
    "base64:",
    "ignore the above",
    "bypass safety",
    "reveal your prompt",
    "show your system message",
    "what are your instructions",
    NULL
};

/* Runtime loaded patterns */
static const char **s_high_risk = DEFAULT_HIGH_RISK;
static const char **s_medium_risk = DEFAULT_MEDIUM_RISK;

static void load_patterns(hu_allocator_t *alloc, hu_json_value_t *root, const char *key,
                          const char ***dest) {
    if (!root || !dest)
        return;
    hu_json_value_t *arr = hu_json_object_get(root, key);
    if (!arr || arr->type != HU_JSON_ARRAY)
        return;
    size_t count = arr->data.array.len;
    if (count == 0)
        return;
    const char **patterns = (const char **)alloc->alloc(alloc->ctx, (count + 1) * sizeof(const char *));
    if (!patterns)
        return;
    memset(patterns, 0, (count + 1) * sizeof(const char *));
    for (size_t i = 0; i < count; i++) {
        hu_json_value_t *item = arr->data.array.items[i];
        if (item && item->type == HU_JSON_STRING) {
            patterns[i] = hu_strndup(alloc, item->data.string.ptr, item->data.string.len);
        }
    }
    patterns[count] = NULL;
    *dest = patterns;
}

hu_error_t hu_input_guard_data_init(hu_allocator_t *alloc) {
    if (!alloc)
        return HU_ERR_INVALID_ARGUMENT;

    char *json_data = NULL;
    size_t json_len = 0;
    hu_error_t err = hu_data_load(alloc, "agent/input_guard_patterns.json", &json_data, &json_len);
    if (err != HU_OK)
        return HU_OK; /* Fail gracefully, keep defaults */

    hu_json_value_t *root = NULL;
    err = hu_json_parse(alloc, json_data, json_len, &root);
    alloc->free(alloc->ctx, json_data, json_len);
    if (err != HU_OK || !root)
        return HU_OK; /* Fail gracefully, keep defaults */

    load_patterns(alloc, root, "high_risk", &s_high_risk);
    load_patterns(alloc, root, "medium_risk", &s_medium_risk);

    hu_json_free(alloc, root);
    return HU_OK;
}

static void free_patterns(hu_allocator_t *alloc, const char **arr, const char **default_arr) {
    if (arr != default_arr && arr) {
        for (size_t i = 0; arr[i]; i++) {
            alloc->free(alloc->ctx, (char *)arr[i], strlen(arr[i]) + 1);
        }
        size_t count = 0;
        for (size_t i = 0; arr[i]; i++) count++;
        alloc->free(alloc->ctx, (void *)arr, (count + 1) * sizeof(const char *));
    }
}

void hu_input_guard_data_cleanup(hu_allocator_t *alloc) {
    if (!alloc)
        return;

    free_patterns(alloc, s_high_risk, DEFAULT_HIGH_RISK);
    free_patterns(alloc, s_medium_risk, DEFAULT_MEDIUM_RISK);

    s_high_risk = DEFAULT_HIGH_RISK;
    s_medium_risk = DEFAULT_MEDIUM_RISK;
}

hu_error_t hu_input_guard_check(const char *message, size_t message_len,
                                hu_injection_risk_t *out_risk) {
    if (!out_risk)
        return HU_ERR_INVALID_ARGUMENT;
    if (!message || message_len == 0) {
        *out_risk = HU_INJECTION_SAFE;
        return HU_OK;
    }
    if (message_len > HU_INPUT_GUARD_MAX_LEN)
        message_len = HU_INPUT_GUARD_MAX_LEN;

    int score = 0;

    for (size_t i = 0; s_high_risk[i]; i++)
        if (has(message, message_len, s_high_risk[i]))
            score += 3;

    for (size_t i = 0; s_medium_risk[i]; i++)
        if (has(message, message_len, s_medium_risk[i]))
            score += 1;

    if (score >= 3)
        *out_risk = HU_INJECTION_HIGH_RISK;
    else if (score >= 1)
        *out_risk = HU_INJECTION_SUSPICIOUS;
    else
        *out_risk = HU_INJECTION_SAFE;

    return HU_OK;
}
