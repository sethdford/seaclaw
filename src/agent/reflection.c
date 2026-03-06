#include "seaclaw/agent/reflection.h"
#include "seaclaw/core/string.h"
#include <string.h>

static bool contains_ci(const char *s, size_t slen, const char *needle, size_t nlen) {
    if (nlen > slen)
        return false;
    for (size_t i = 0; i <= slen - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            char a = s[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z')
                a += 32;
            if (b >= 'A' && b <= 'Z')
                b += 32;
            if (a != b) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}

sc_reflection_quality_t sc_reflection_evaluate(const char *user_query, size_t user_query_len,
                                               const char *response, size_t response_len,
                                               const sc_reflection_config_t *config) {
    if (!response || response_len == 0)
        return SC_QUALITY_NEEDS_RETRY;

    /* Check for empty/trivial responses */
    if (response_len < 10)
        return SC_QUALITY_NEEDS_RETRY;

    /* Check for known failure patterns */
    if (contains_ci(response, response_len, "i cannot", 8) ||
        contains_ci(response, response_len, "i can't", 7) ||
        contains_ci(response, response_len, "i'm unable", 10) ||
        contains_ci(response, response_len, "as an ai", 8))
        return SC_QUALITY_ACCEPTABLE;

    /* If user asked a question, check response addresses it */
    bool user_has_question = false;
    if (user_query && user_query_len > 0) {
        for (size_t i = 0; i < user_query_len; i++) {
            if (user_query[i] == '?') {
                user_has_question = true;
                break;
            }
        }
    }

    if (user_has_question && response_len < 30)
        return SC_QUALITY_ACCEPTABLE;

    /* Min response tokens check (approximate tokens as words) */
    if (config && config->min_response_tokens > 0) {
        int approx_words = 1;
        for (size_t i = 0; i < response_len; i++)
            if (response[i] == ' ')
                approx_words++;
        if (approx_words < config->min_response_tokens)
            return SC_QUALITY_ACCEPTABLE;
    }

    return SC_QUALITY_GOOD;
}

sc_error_t sc_reflection_build_critique_prompt(sc_allocator_t *alloc, const char *user_query,
                                               size_t user_query_len, const char *response,
                                               size_t response_len, char **out_prompt,
                                               size_t *out_prompt_len) {
    if (!alloc || !out_prompt)
        return SC_ERR_INVALID_ARGUMENT;

    const char *prefix =
        "Evaluate the following response. Score it as GOOD, ACCEPTABLE, or NEEDS_RETRY. "
        "If NEEDS_RETRY, explain what should be improved.\n\n"
        "User query: ";
    size_t prefix_len = strlen(prefix);

    const char *mid = "\n\nResponse: ";
    size_t mid_len = strlen(mid);

    const char *suffix = "\n\nEvaluation:";
    size_t suffix_len = strlen(suffix);

    size_t total = prefix_len + user_query_len + mid_len + response_len + suffix_len;
    char *buf = (char *)alloc->alloc(alloc->ctx, total + 1);
    if (!buf)
        return SC_ERR_OUT_OF_MEMORY;

    size_t pos = 0;
    memcpy(buf + pos, prefix, prefix_len);
    pos += prefix_len;
    if (user_query && user_query_len > 0) {
        memcpy(buf + pos, user_query, user_query_len);
        pos += user_query_len;
    }
    memcpy(buf + pos, mid, mid_len);
    pos += mid_len;
    if (response && response_len > 0) {
        memcpy(buf + pos, response, response_len);
        pos += response_len;
    }
    memcpy(buf + pos, suffix, suffix_len);
    pos += suffix_len;
    buf[pos] = '\0';

    *out_prompt = buf;
    if (out_prompt_len)
        *out_prompt_len = pos;
    return SC_OK;
}

void sc_reflection_result_free(sc_allocator_t *alloc, sc_reflection_result_t *result) {
    if (!alloc || !result)
        return;
    if (result->feedback && result->feedback_len > 0)
        alloc->free(alloc->ctx, result->feedback, result->feedback_len + 1);
    result->feedback = NULL;
    result->feedback_len = 0;
}
