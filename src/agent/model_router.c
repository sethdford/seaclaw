#include "human/agent/model_router.h"
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

static bool ci_contains(const char *haystack, size_t haystack_len, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen > haystack_len)
        return false;
    for (size_t i = 0; i <= haystack_len - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}

static bool has_question(const char *msg, size_t len) {
    for (size_t i = 0; i < len; i++)
        if (msg[i] == '?')
            return true;
    return false;
}

static size_t word_count(const char *msg, size_t len) {
    if (!msg || len == 0)
        return 0;
    size_t count = 1;
    for (size_t i = 0; i < len; i++)
        if (msg[i] == ' ')
            count++;
    return count;
}

/* Detect emotional weight — messages that need empathy, not speed */
static int emotional_weight(const char *msg, size_t len) {
    int score = 0;
    static const char *heavy[] = {
        "died", "dying", "cancer", "funeral", "divorce", "breakup", "depressed",
        "suicidal", "hospital", "emergency", "scared", "terrified", "heartbroken",
        "lost my", "passed away", "miss you", "love you", "worried about",
        "don't know what to do", "need help", "struggling", "overwhelmed",
        "can't sleep", "crying", "panic", "anxiety", "therapy"
    };
    static const char *moderate[] = {
        "frustrated", "stressed", "upset", "annoyed", "confused", "angry",
        "disappointed", "tired", "exhausted", "sick", "hurt", "lonely",
        "nervous", "worried", "sorry", "ugh", "hate", "awful", "terrible",
        "not working", "broken", "failing"
    };
    for (size_t i = 0; i < sizeof(heavy) / sizeof(heavy[0]); i++)
        if (ci_contains(msg, len, heavy[i]))
            score += 3;
    for (size_t i = 0; i < sizeof(moderate) / sizeof(moderate[0]); i++)
        if (ci_contains(msg, len, moderate[i]))
            score += 1;
    return score;
}

/* Detect advice-seeking or complex reasoning needs */
static bool needs_reasoning(const char *msg, size_t len) {
    static const char *markers[] = {
        "should i", "what do you think", "what would you", "how do i",
        "help me decide", "pros and cons", "advice", "opinion",
        "compared to", "better option", "worth it", "trade-off",
        "explain", "why does", "how does", "what if"
    };
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++)
        if (ci_contains(msg, len, markers[i]))
            return true;
    return false;
}

/* Relationship closeness: family and close friends get higher-quality models */
static int relationship_weight(const char *rel, size_t rel_len) {
    if (!rel || rel_len == 0)
        return 0;
    if (rel_len == 6 && memcmp(rel, "family", 6) == 0)
        return 2;
    if (rel_len >= 5 && memcmp(rel, "close", 5) == 0)
        return 2;
    if ((rel_len == 3 && memcmp(rel, "mom", 3) == 0) ||
        (rel_len == 3 && memcmp(rel, "dad", 3) == 0) ||
        (rel_len == 6 && memcmp(rel, "mother", 6) == 0) ||
        (rel_len == 6 && memcmp(rel, "father", 6) == 0) ||
        (rel_len == 6 && memcmp(rel, "sister", 6) == 0) ||
        (rel_len == 7 && memcmp(rel, "brother", 7) == 0) ||
        (rel_len == 6 && memcmp(rel, "spouse", 6) == 0) ||
        (rel_len == 7 && memcmp(rel, "partner", 7) == 0))
        return 2;
    if (rel_len == 6 && memcmp(rel, "friend", 6) == 0)
        return 1;
    return 0;
}

hu_model_router_config_t hu_model_router_default_config(void) {
    hu_model_router_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.reflexive_model = "gemini-3.1-flash-lite-preview";
    cfg.reflexive_model_len = 29;
    cfg.conversational_model = "gemini-3-flash-preview";
    cfg.conversational_model_len = 22;
    cfg.analytical_model = "gemini-3.1-pro-preview";
    cfg.analytical_model_len = 22;
    cfg.deep_model = "gemini-3.1-pro-preview";
    cfg.deep_model_len = 22;
    return cfg;
}

hu_model_selection_t hu_model_route(const hu_model_router_config_t *cfg,
                                    const char *msg, size_t msg_len,
                                    const char *relationship, size_t relationship_len,
                                    int hour, size_t history_count) {
    hu_model_selection_t sel;
    memset(&sel, 0, sizeof(sel));

    if (!cfg || !msg || msg_len == 0) {
        sel.model = cfg ? cfg->conversational_model : "gemini-3-flash-preview";
        sel.model_len = cfg ? cfg->conversational_model_len : 22;
        sel.tier = HU_TIER_CONVERSATIONAL;
        return sel;
    }

    size_t words = word_count(msg, msg_len);
    int emotion = emotional_weight(msg, msg_len);
    bool question = has_question(msg, msg_len);
    bool reasoning = needs_reasoning(msg, msg_len);
    int rel_w = relationship_weight(relationship, relationship_len);

    /* Score accumulation: higher = needs more capable model */
    int score = 0;

    /* Message characteristics */
    if (words <= 3)
        score -= 2;
    else if (words <= 8)
        score -= 1;
    else if (words > 30)
        score += 1;
    else if (words > 80)
        score += 2;

    score += emotion;

    /* Emotional messages always deserve a capable model regardless of length */
    if (emotion >= 3)
        score += 3;

    if (question)
        score += 1;
    if (reasoning)
        score += 2;

    /* Relationship boost: family/close = higher quality */
    score += rel_w;

    /* Long conversation = more context needed */
    if (history_count > 6)
        score += 1;

    /* Late-night emotional = probably important */
    if ((hour >= 23 || hour <= 4) && emotion > 0)
        score += 1;

    /* Route to tier */
    if (score <= 0) {
        sel.tier = HU_TIER_REFLEXIVE;
        sel.model = cfg->reflexive_model;
        sel.model_len = cfg->reflexive_model_len;
        sel.thinking_budget = 0;
        sel.temperature = 0.9;
    } else if (score <= 3) {
        sel.tier = HU_TIER_CONVERSATIONAL;
        sel.model = cfg->conversational_model;
        sel.model_len = cfg->conversational_model_len;
        sel.thinking_budget = 1024;
        sel.temperature = 0.8;
    } else if (score <= 6) {
        sel.tier = HU_TIER_ANALYTICAL;
        sel.model = cfg->analytical_model;
        sel.model_len = cfg->analytical_model_len;
        sel.thinking_budget = 4096;
        sel.temperature = 0.7;
    } else {
        sel.tier = HU_TIER_DEEP;
        sel.model = cfg->deep_model;
        sel.model_len = cfg->deep_model_len;
        sel.thinking_budget = 8192;
        sel.temperature = 0.6;
    }

    return sel;
}
