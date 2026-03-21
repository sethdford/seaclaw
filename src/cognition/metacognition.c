#include "human/cognition/metacognition.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

void hu_metacognition_init(hu_metacognition_t *mc) {
    if (!mc) return;
    memset(mc, 0, sizeof(*mc));
    mc->max_reflects = 2;
    mc->confidence_threshold = 0.3f;
    mc->coherence_threshold = 0.2f;
    mc->repetition_threshold = 0.6f;
}

/* Count occurrences of hedging words in text to estimate confidence. */
static float estimate_confidence(const char *text, size_t len) {
    if (!text || len == 0) return 0.5f;

    static const char *hedges[] = {
        "maybe", "perhaps", "possibly", "might", "could be",
        "i think", "i believe", "not sure", "uncertain",
        "it seems", "arguably", "however", "although",
        NULL
    };

    size_t hedge_count = 0;
    size_t word_count = 0;
    bool in_word = false;

    /* Count words */
    for (size_t i = 0; i < len; i++) {
        if (isalpha((unsigned char)text[i])) {
            if (!in_word) { word_count++; in_word = true; }
        } else {
            in_word = false;
        }
    }
    if (word_count == 0) return 0.5f;

    /* Count hedge phrases (case-insensitive substring match) */
    char lower[4096];
    size_t copy_len = len < sizeof(lower) - 1 ? len : sizeof(lower) - 1;
    for (size_t i = 0; i < copy_len; i++)
        lower[i] = (char)tolower((unsigned char)text[i]);
    lower[copy_len] = '\0';

    for (size_t i = 0; hedges[i]; i++) {
        const char *p = lower;
        while ((p = strstr(p, hedges[i])) != NULL) {
            hedge_count++;
            p += strlen(hedges[i]);
        }
    }

    float hedge_ratio = (float)hedge_count / (float)word_count;
    float conf = 1.0f - hedge_ratio * 10.0f;
    if (conf < 0.0f) conf = 0.0f;
    if (conf > 1.0f) conf = 1.0f;
    return conf;
}

/* Compute word overlap ratio between response and user query. */
static float estimate_coherence(const char *query, size_t qlen,
                                 const char *response, size_t rlen) {
    if (!query || qlen == 0 || !response || rlen == 0) return 0.0f;

    char q_lower[2048];
    size_t ql = qlen < sizeof(q_lower) - 1 ? qlen : sizeof(q_lower) - 1;
    for (size_t i = 0; i < ql; i++)
        q_lower[i] = (char)tolower((unsigned char)query[i]);
    q_lower[ql] = '\0';

    char r_lower[4096];
    size_t rl = rlen < sizeof(r_lower) - 1 ? rlen : sizeof(r_lower) - 1;
    for (size_t i = 0; i < rl; i++)
        r_lower[i] = (char)tolower((unsigned char)response[i]);
    r_lower[rl] = '\0';

    /* Extract query words and check presence in response */
    size_t matches = 0, total = 0;
    const char *p = q_lower;
    while (*p) {
        while (*p && !isalpha((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && isalpha((unsigned char)*p)) p++;
        size_t wlen = (size_t)(p - start);
        if (wlen < 3) continue;
        total++;

        char word[64];
        if (wlen >= sizeof(word)) wlen = sizeof(word) - 1;
        memcpy(word, start, wlen);
        word[wlen] = '\0';

        if (strstr(r_lower, word)) matches++;
    }

    if (total == 0) return 0.5f;
    return (float)matches / (float)total;
}

/* Compute bigram overlap between current and previous response. */
static float estimate_repetition(const char *current, size_t clen,
                                  const char *prev, size_t plen) {
    if (!current || clen == 0 || !prev || plen == 0) return 0.0f;

    /* Simple approach: count shared 4-char shingles */
    if (clen < 4 || plen < 4) return 0.0f;

    size_t max_check = clen < 2000 ? clen : 2000;
    size_t matches = 0;
    size_t total = 0;

    for (size_t i = 0; i + 4 <= max_check; i += 4) {
        total++;
        for (size_t j = 0; j + 4 <= plen; j++) {
            if (memcmp(current + i, prev + j, 4) == 0) {
                matches++;
                break;
            }
        }
    }

    if (total == 0) return 0.0f;
    return (float)matches / (float)total;
}

hu_metacognition_signal_t hu_metacognition_monitor(
    const char *user_query, size_t user_query_len,
    const char *response, size_t response_len,
    const char *prev_response, size_t prev_response_len,
    float emotional_confidence,
    uint64_t input_tokens, uint64_t output_tokens) {

    hu_metacognition_signal_t s;
    memset(&s, 0, sizeof(s));

    s.confidence = estimate_confidence(response, response_len);
    s.coherence = estimate_coherence(user_query, user_query_len, response, response_len);
    s.repetition = estimate_repetition(response, response_len,
                                        prev_response, prev_response_len);
    s.emotional_alignment = emotional_confidence;

    if (input_tokens > 0)
        s.token_efficiency = (float)output_tokens / (float)input_tokens;
    else
        s.token_efficiency = 0.0f;

    return s;
}

hu_metacog_action_t hu_metacognition_plan_action(hu_metacognition_t *mc,
                                                  const hu_metacognition_signal_t *signal) {
    if (!mc || !signal) return HU_METACOG_ACTION_NONE;

    hu_metacognition_record_signal(mc, signal);

    /* High repetition -> simplify to break the loop */
    if (signal->repetition > mc->repetition_threshold) {
        mc->last_action = HU_METACOG_ACTION_SIMPLIFY;
        return HU_METACOG_ACTION_SIMPLIFY;
    }

    /* Low coherence -> switch strategy (different approach needed) */
    if (signal->coherence < mc->coherence_threshold) {
        mc->last_action = HU_METACOG_ACTION_SWITCH_STRATEGY;
        return HU_METACOG_ACTION_SWITCH_STRATEGY;
    }

    /* Low confidence -> reflect (if budget allows) or clarify */
    if (signal->confidence < mc->confidence_threshold) {
        if (mc->reflect_count < mc->max_reflects) {
            mc->reflect_count++;
            mc->last_action = HU_METACOG_ACTION_REFLECT;
            return HU_METACOG_ACTION_REFLECT;
        }
        mc->last_action = HU_METACOG_ACTION_CLARIFY;
        return HU_METACOG_ACTION_CLARIFY;
    }

    /* Token efficiency too low (verbose response) -> simplify */
    if (signal->token_efficiency > 2.0f) {
        mc->last_action = HU_METACOG_ACTION_SIMPLIFY;
        return HU_METACOG_ACTION_SIMPLIFY;
    }

    mc->last_action = HU_METACOG_ACTION_NONE;
    return HU_METACOG_ACTION_NONE;
}

void hu_metacognition_record_signal(hu_metacognition_t *mc,
                                    const hu_metacognition_signal_t *signal) {
    if (!mc || !signal) return;
    mc->signals[mc->signal_idx] = *signal;
    mc->signal_idx = (mc->signal_idx + 1) % HU_METACOG_SIGNAL_RING_SIZE;
    if (mc->signal_count < HU_METACOG_SIGNAL_RING_SIZE)
        mc->signal_count++;
}

const char *hu_metacog_action_name(hu_metacog_action_t action) {
    switch (action) {
    case HU_METACOG_ACTION_NONE:            return "none";
    case HU_METACOG_ACTION_REFLECT:         return "reflect";
    case HU_METACOG_ACTION_DEEPEN:          return "deepen";
    case HU_METACOG_ACTION_SIMPLIFY:        return "simplify";
    case HU_METACOG_ACTION_CLARIFY:         return "clarify";
    case HU_METACOG_ACTION_SWITCH_STRATEGY: return "switch_strategy";
    default:                                return "unknown";
    }
}
