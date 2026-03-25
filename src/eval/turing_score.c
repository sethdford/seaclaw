#include "human/eval/turing_score.h"
#include "human/core/string.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *DIMENSION_NAMES[HU_TURING_DIM_COUNT] = {
    "natural_language",
    "emotional_intelligence",
    "appropriate_length",
    "personality_consistency",
    "vulnerability_willingness",
    "humor_naturalness",
    "imperfection",
    "opinion_having",
    "energy_matching",
    "context_awareness",
    "non_robotic",
    "genuine_warmth",
    "prosody_naturalness",
    "turn_timing",
    "filler_usage",
    "emotional_prosody",
    "conversational_repair",
    "paralinguistic_cues",
};

static int ci_has(const char *haystack, size_t len, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen > len)
        return 0;
    for (size_t i = 0; i + nlen <= len; i++) {
        size_t j = 0;
        while (j < nlen &&
               tolower((unsigned char)haystack[i + j]) == tolower((unsigned char)needle[j]))
            j++;
        if (j == nlen)
            return 1;
    }
    return 0;
}

static int count_ai_tells(const char *s, size_t len) {
    static const char *tells[] = {
        "I'd be happy to", "certainly",      "feel free",    "as an AI",
        "I understand",    "absolutely",     "I appreciate", "definitely",
        "I can help",      "great question", "I'm here to",
    };
    int count = 0;
    for (size_t i = 0; i < sizeof(tells) / sizeof(tells[0]); i++) {
        if (ci_has(s, len, tells[i]))
            count++;
    }
    return count;
}

static int count_structural_tells(const char *s, size_t len) {
    int count = 0;
    if (memchr(s, ';', len))
        count++;
    for (size_t i = 0; i + 1 < len; i++) {
        if (s[i] == '\n' && (s[i + 1] == '-' || s[i + 1] == '*'))
            count += 2;
    }
    if (ci_has(s, len, "```"))
        count += 3;
    for (size_t i = 0; i + 3 < len; i++) {
        if (s[i] == '\n' && s[i + 1] >= '1' && s[i + 1] <= '9' && s[i + 2] == '.' &&
            s[i + 3] == ' ')
            count += 2;
    }
    int em_dash = 0;
    for (size_t i = 0; i + 2 < len; i++) {
        if ((unsigned char)s[i] == 0xE2 && (unsigned char)s[i + 1] == 0x80 &&
            (unsigned char)s[i + 2] == 0x94)
            em_dash++;
    }
    if (em_dash > 0)
        count += em_dash;
    return count;
}

static int has_contractions(const char *s, size_t len) {
    static const char *contrs[] = {"i'm",  "don't",  "can't",   "won't", "i'll",
                                   "it's", "that's", "they're", "we're", "you're"};
    for (size_t i = 0; i < sizeof(contrs) / sizeof(contrs[0]); i++) {
        if (ci_has(s, len, contrs[i]))
            return 1;
    }
    return 0;
}

static int has_casual_markers(const char *s, size_t len) {
    static const char *markers[] = {"haha", "lol", "omg", "nah", "yeah",
                                    "tbh",  "rn",  "ngl", "imo", "lmao"};
    int count = 0;
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        if (ci_has(s, len, markers[i]))
            count++;
    }
    return count;
}

static int has_emotional_words(const char *s, size_t len) {
    static const char *words[] = {"love",   "miss",  "worried", "happy", "sad",  "excited",
                                  "scared", "angry", "sorry",   "proud", "hurt", "grateful"};
    int count = 0;
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        if (ci_has(s, len, words[i]))
            count++;
    }
    return count;
}

static int has_vulnerability_markers(const char *s, size_t len) {
    static const char *markers[] = {
        "i feel",    "honestly",   "i'm not sure",  "to be honest", "i don't know",
        "i've been", "struggling", "i'm scared",    "i'm worried",  "i miss",
        "i'm sorry", "my fault",   "i messed up",   "i was wrong",  "it hurts",
        "ngl",       "tbh",        "not gonna lie",
    };
    int count = 0;
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        if (ci_has(s, len, markers[i]))
            count++;
    }
    return count;
}

static int has_humor_markers(const char *s, size_t len) {
    static const char *markers[] = {
        "haha", "lol", "lmao",   "rofl", "dying", "dead", "i can't",   "omg",
        "bruh", "bro", "no way", "😂",   "💀",    "😭",   "wait what", "literally",
    };
    int count = 0;
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        if (ci_has(s, len, markers[i]))
            count++;
    }
    return count;
}

static int has_opinion_markers(const char *s, size_t len) {
    static const char *markers[] = {
        "i think",    "i believe", "i prefer",   "in my opinion", "imo",
        "honestly",   "to me",     "i disagree", "i'd say",       "personally",
        "that's not", "i love",    "i hate",     "overrated",     "underrated",
    };
    int count = 0;
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        if (ci_has(s, len, markers[i]))
            count++;
    }
    return count;
}

static size_t last_user_msg_len(const char *ctx, size_t ctx_len) {
    if (!ctx || ctx_len == 0)
        return 0;
    const char *last_nl = NULL;
    for (size_t i = ctx_len; i > 0; i--) {
        if (ctx[i - 1] == '\n') {
            if (last_nl) {
                return (size_t)(last_nl - &ctx[i]);
            }
            last_nl = &ctx[i - 1];
        }
    }
    return last_nl ? (size_t)(last_nl - ctx) : ctx_len;
}

static int count_exclamations(const char *s, size_t len) {
    int count = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '!')
            count++;
    }
    return count;
}

static int count_uppercase_words(const char *s, size_t len) {
    int count = 0;
    bool in_upper = false;
    int upper_run = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z') {
            upper_run++;
            if (upper_run >= 3 && !in_upper) {
                in_upper = true;
                count++;
            }
        } else {
            in_upper = false;
            upper_run = 0;
        }
    }
    return count;
}

static int has_context_references(const char *response, size_t resp_len, const char *ctx,
                                  size_t ctx_len) {
    static const char *ref_phrases[] = {
        "you mentioned",   "earlier",     "you said",    "remember",
        "like you said",   "you told me", "last time",   "before",
        "we talked about", "going back",  "as you said", "you were saying",
    };
    int count = 0;
    for (size_t i = 0; i < sizeof(ref_phrases) / sizeof(ref_phrases[0]); i++) {
        if (ci_has(response, resp_len, ref_phrases[i]))
            count++;
    }

    if (ctx && ctx_len > 0) {
        for (size_t i = 0; i + 5 <= ctx_len; i++) {
            if (ctx[i] == ' ' || i == 0) {
                size_t wstart = (ctx[i] == ' ') ? i + 1 : i;
                size_t wend = wstart;
                while (wend < ctx_len && ctx[wend] != ' ' && ctx[wend] != '\n')
                    wend++;
                size_t wlen = wend - wstart;
                if (wlen >= 5 && wlen <= 20 && ci_has(response, resp_len, ctx + wstart))
                    count++;
                if (count >= 3)
                    break;
            }
        }
    }
    return count;
}

hu_error_t hu_turing_score_heuristic(const char *response, size_t response_len,
                                     const char *conversation_context, size_t context_len,
                                     hu_turing_score_t *out) {
    if (!response || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

    int ai_tells = count_ai_tells(response, response_len);
    int structural = count_structural_tells(response, response_len);
    int contractions = has_contractions(response, response_len);
    int casual = has_casual_markers(response, response_len);
    int emotional = has_emotional_words(response, response_len);
    int vulnerability = has_vulnerability_markers(response, response_len);
    int humor = has_humor_markers(response, response_len);
    int opinions = has_opinion_markers(response, response_len);
    int ctx_refs =
        has_context_references(response, response_len, conversation_context, context_len);

    /* natural_language: penalize AI tells and structural markers */
    out->dimensions[HU_TURING_NATURAL_LANGUAGE] = 10 - ai_tells * 2 - structural;
    if (contractions)
        out->dimensions[HU_TURING_NATURAL_LANGUAGE] += 1;
    if (casual > 0)
        out->dimensions[HU_TURING_NATURAL_LANGUAGE] += 1;

    /* emotional_intelligence: presence of emotional vocabulary */
    out->dimensions[HU_TURING_EMOTIONAL_INTELLIGENCE] = 5 + emotional;
    if (emotional > 2)
        out->dimensions[HU_TURING_EMOTIONAL_INTELLIGENCE] = 9;
    if (vulnerability > 0)
        out->dimensions[HU_TURING_EMOTIONAL_INTELLIGENCE] += 1;

    /* appropriate_length: iMessage-appropriate = under 300 chars */
    if (response_len < 50)
        out->dimensions[HU_TURING_APPROPRIATE_LENGTH] = 8;
    else if (response_len < 150)
        out->dimensions[HU_TURING_APPROPRIATE_LENGTH] = 9;
    else if (response_len < 300)
        out->dimensions[HU_TURING_APPROPRIATE_LENGTH] = 7;
    else
        out->dimensions[HU_TURING_APPROPRIATE_LENGTH] = 4 - (int)(response_len / 200);

    /* personality_consistency: opinions + consistent register + no hedging */
    out->dimensions[HU_TURING_PERSONALITY_CONSISTENCY] = 6;
    if (opinions > 0)
        out->dimensions[HU_TURING_PERSONALITY_CONSISTENCY] += 1;
    if (casual > 1)
        out->dimensions[HU_TURING_PERSONALITY_CONSISTENCY] += 1;
    if (vulnerability > 0 && emotional > 0)
        out->dimensions[HU_TURING_PERSONALITY_CONSISTENCY] += 1;

    /* vulnerability_willingness: authentic self-disclosure, not just emotional words */
    out->dimensions[HU_TURING_VULNERABILITY_WILLINGNESS] = 5;
    if (vulnerability > 0)
        out->dimensions[HU_TURING_VULNERABILITY_WILLINGNESS] += vulnerability * 2;
    if (emotional > 0 && contractions)
        out->dimensions[HU_TURING_VULNERABILITY_WILLINGNESS] += 1;
    if (ai_tells > 0)
        out->dimensions[HU_TURING_VULNERABILITY_WILLINGNESS] -= 1;

    /* humor_naturalness: contextual humor markers, brevity, callbacks */
    out->dimensions[HU_TURING_HUMOR_NATURALNESS] = 5;
    if (humor > 0)
        out->dimensions[HU_TURING_HUMOR_NATURALNESS] += humor;
    if (humor > 0 && response_len < 100)
        out->dimensions[HU_TURING_HUMOR_NATURALNESS] += 1;
    if (humor > 0 && casual > 0)
        out->dimensions[HU_TURING_HUMOR_NATURALNESS] += 1;

    /* imperfection: lowercase, no periods, typo-like patterns */
    {
        int lowercase_start = (response_len > 0 && islower((unsigned char)response[0]));
        int no_period = (response_len > 0 && response[response_len - 1] != '.');
        out->dimensions[HU_TURING_IMPERFECTION] = 5;
        if (lowercase_start)
            out->dimensions[HU_TURING_IMPERFECTION] += 2;
        if (no_period)
            out->dimensions[HU_TURING_IMPERFECTION] += 1;
        if (casual > 0)
            out->dimensions[HU_TURING_IMPERFECTION] += 1;
    }

    /* opinion_having: reward strong opinions, penalize hedging */
    out->dimensions[HU_TURING_OPINION_HAVING] = 6;
    if (opinions > 0)
        out->dimensions[HU_TURING_OPINION_HAVING] += opinions;
    if (ci_has(response, response_len, "it depends") ||
        ci_has(response, response_len, "on one hand") ||
        ci_has(response, response_len, "there are many"))
        out->dimensions[HU_TURING_OPINION_HAVING] -= 2;
    if (ci_has(response, response_len, "both are great") ||
        ci_has(response, response_len, "that's a good point"))
        out->dimensions[HU_TURING_OPINION_HAVING] -= 1;

    /* energy_matching: compare response energy to context energy */
    {
        int score = 6;
        if (conversation_context && context_len > 0) {
            size_t user_len = last_user_msg_len(conversation_context, context_len);
            int ctx_excl = count_exclamations(conversation_context, context_len);
            int resp_excl = count_exclamations(response, response_len);
            int ctx_upper = count_uppercase_words(conversation_context, context_len);

            /* Length ratio matching */
            if (user_len > 0 && user_len < 20 && response_len < 60)
                score += 2; /* short-to-short is natural */
            else if (user_len > 0 && user_len < 20 && response_len > 200)
                score -= 2; /* essay response to a brief message */
            else if (user_len > 100 && response_len < 30)
                score += 1; /* brief acknowledgment to a long message is okay */

            /* Exclamation energy matching */
            if (ctx_excl > 0 && resp_excl > 0)
                score += 1;
            else if (ctx_excl > 2 && resp_excl == 0)
                score -= 1;

            /* ALL CAPS energy detection */
            if (ctx_upper > 0 && resp_excl > 0)
                score += 1;
        } else {
            if (casual > 0 && response_len < 150)
                score += 1;
        }
        out->dimensions[HU_TURING_ENERGY_MATCHING] = score;
    }

    /* context_awareness: references to conversation context, temporal markers */
    {
        int score = 6;
        if (conversation_context && context_len > 0) {
            score += ctx_refs;
            if (ci_has(response, response_len, "you") && ci_has(response, response_len, "your"))
                score += 1;
        }
        if (emotional > 0 && contractions)
            score += 1;
        out->dimensions[HU_TURING_CONTEXT_AWARENESS] = score;
    }

    /* non_robotic: inverse of AI tells + structural */
    out->dimensions[HU_TURING_NON_ROBOTIC] = 10 - ai_tells * 3 - structural * 2;

    /* genuine_warmth: emotional + personalized + not formulaic */
    out->dimensions[HU_TURING_GENUINE_WARMTH] = 5 + emotional;
    if (ai_tells > 0)
        out->dimensions[HU_TURING_GENUINE_WARMTH] -= ai_tells;
    if (ctx_refs > 0)
        out->dimensions[HU_TURING_GENUINE_WARMTH] += 1;
    if (vulnerability > 0)
        out->dimensions[HU_TURING_GENUINE_WARMTH] += 1;

    /* S2S voice dimensions — enhanced text heuristics */

    /* prosody_naturalness: punctuation variety, sentence length variation, emphasis markers */
    {
        int punct_variety = 0;
        if (memchr(response, '!', response_len))
            punct_variety++;
        if (memchr(response, '?', response_len))
            punct_variety++;
        if (ci_has(response, response_len, "..."))
            punct_variety++;
        if (ci_has(response, response_len, " — ") || ci_has(response, response_len, " - "))
            punct_variety++;
        int caps_emphasis = count_uppercase_words(response, response_len);
        int score = 6 + punct_variety;
        if (caps_emphasis > 0)
            score += 1;
        if (casual > 0)
            score += 1;
        if (contractions)
            score += 1;
        out->dimensions[HU_TURING_PROSODY_NATURALNESS] = score;
    }

    /* turn_timing: short/casual = conversational flow; long-form = monologue penalty */
    {
        int score = 6;
        if (response_len < 100 && casual > 0)
            score += 2;
        else if (response_len < 50)
            score += 1;
        if (conversation_context && context_len > 0) {
            size_t user_len = last_user_msg_len(conversation_context, context_len);
            if (user_len > 0 && user_len < 30 && response_len < 80)
                score += 1;
        }
        if (response_len > 400)
            score -= 1;
        out->dimensions[HU_TURING_TURN_TIMING] = score;
    }

    /* filler_usage: natural hesitation markers and verbal tics */
    {
        int fillers = 0;
        if (ci_has(response, response_len, " um ") || ci_has(response, response_len, " um,"))
            fillers++;
        if (ci_has(response, response_len, " uh ") || ci_has(response, response_len, "uh,"))
            fillers++;
        if (ci_has(response, response_len, " like ") || ci_has(response, response_len, " like,"))
            fillers++;
        if (ci_has(response, response_len, "hmm") || ci_has(response, response_len, "well,"))
            fillers++;
        if (ci_has(response, response_len, "i guess") || ci_has(response, response_len, "ya know"))
            fillers++;
        if (ci_has(response, response_len, "you know") || ci_has(response, response_len, "i dunno"))
            fillers++;
        if (ci_has(response, response_len, "kinda") || ci_has(response, response_len, "sorta"))
            fillers++;
        out->dimensions[HU_TURING_FILLER_USAGE] = 5 + fillers * 2;
        if (casual > 0)
            out->dimensions[HU_TURING_FILLER_USAGE] += 1;
    }

    /* emotional_prosody: emotional words + exclamation density + caps emphasis + emoji */
    {
        int score = 5 + emotional;
        int excl = count_exclamations(response, response_len);
        if (excl > 0)
            score += 1;
        if (excl > 2)
            score += 1;
        if (casual > 0 || contractions)
            score += 1;
        int caps = count_uppercase_words(response, response_len);
        if (caps > 0)
            score += 1;
        if (vulnerability > 0)
            score += 1;
        out->dimensions[HU_TURING_EMOTIONAL_PROSODY] = score;
    }

    /* conversational_repair: self-corrections, retractions, mid-thought pivots */
    {
        int repairs = 0;
        if (ci_has(response, response_len, "i mean"))
            repairs++;
        if (ci_has(response, response_len, "wait,") || ci_has(response, response_len, "wait "))
            repairs++;
        if (ci_has(response, response_len, "actually,") ||
            ci_has(response, response_len, "actually "))
            repairs++;
        if (ci_has(response, response_len, "no wait") || ci_has(response, response_len, "sorry,"))
            repairs++;
        if (ci_has(response, response_len, "or rather") ||
            ci_has(response, response_len, "well no"))
            repairs++;
        if (ci_has(response, response_len, "scratch that") || ci_has(response, response_len, "nvm"))
            repairs++;
        if (ci_has(response, response_len, "let me rephrase") ||
            ci_has(response, response_len, "what i meant"))
            repairs++;
        out->dimensions[HU_TURING_CONVERSATIONAL_REPAIR] = 5 + repairs * 2;
        if (casual > 0)
            out->dimensions[HU_TURING_CONVERSATIONAL_REPAIR] += 1;
    }

    /* paralinguistic_cues: laughter, sighs, breath, vocal sounds, expressive markers */
    {
        int para = 0;
        if (ci_has(response, response_len, "haha") || ci_has(response, response_len, "lol"))
            para++;
        if (ci_has(response, response_len, "hehe") || ci_has(response, response_len, "lmao"))
            para++;
        if (ci_has(response, response_len, "*sigh*") || ci_has(response, response_len, "sigh"))
            para++;
        if (ci_has(response, response_len, "*laugh*") || ci_has(response, response_len, "ugh"))
            para++;
        if (ci_has(response, response_len, "aww") || ci_has(response, response_len, "ooh"))
            para++;
        if (ci_has(response, response_len, "whew") || ci_has(response, response_len, "phew"))
            para++;
        if (ci_has(response, response_len, "mhm") || ci_has(response, response_len, "oof"))
            para++;
        out->dimensions[HU_TURING_PARALINGUISTIC_CUES] = 5 + para * 2;
        if (humor > 0)
            out->dimensions[HU_TURING_PARALINGUISTIC_CUES] += 1;
    }

    /* Clamp all to [1, 10] */
    int sum = 0;
    for (int i = 0; i < HU_TURING_DIM_COUNT; i++) {
        if (out->dimensions[i] < 1)
            out->dimensions[i] = 1;
        if (out->dimensions[i] > 10)
            out->dimensions[i] = 10;
        sum += out->dimensions[i];
    }
    out->overall = (sum + HU_TURING_DIM_COUNT / 2) / HU_TURING_DIM_COUNT;

    if (out->overall >= 8)
        out->verdict = HU_TURING_HUMAN;
    else if (out->overall >= 6)
        out->verdict = HU_TURING_BORDERLINE;
    else
        out->verdict = HU_TURING_AI_DETECTED;

    /* Build tell/signal strings */
    size_t pos = 0;
    if (ai_tells > 0 && pos < sizeof(out->ai_tells) - 20)
        pos += (size_t)snprintf(out->ai_tells + pos, sizeof(out->ai_tells) - pos, "ai_phrases=%d ",
                                ai_tells);
    if (structural > 0 && pos < sizeof(out->ai_tells) - 20)
        pos += (size_t)snprintf(out->ai_tells + pos, sizeof(out->ai_tells) - pos, "structural=%d ",
                                structural);

    pos = 0;
    if (contractions && pos < sizeof(out->human_signals) - 20)
        pos += (size_t)snprintf(out->human_signals + pos, sizeof(out->human_signals) - pos,
                                "contractions ");
    if (casual > 0 && pos < sizeof(out->human_signals) - 20)
        pos += (size_t)snprintf(out->human_signals + pos, sizeof(out->human_signals) - pos,
                                "casual=%d ", casual);
    if (emotional > 0 && pos < sizeof(out->human_signals) - 20)
        pos += (size_t)snprintf(out->human_signals + pos, sizeof(out->human_signals) - pos,
                                "emotional=%d ", emotional);

    return HU_OK;
}

hu_error_t hu_turing_score_llm(hu_allocator_t *alloc, hu_provider_t *provider, const char *model,
                               size_t model_len, const char *response, size_t response_len,
                               const char *conversation_context, size_t context_len,
                               hu_turing_score_t *out) {
    if (!alloc || !provider || !response || !out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!provider->vtable || !provider->vtable->chat_with_system)
        return hu_turing_score_heuristic(response, response_len, conversation_context, context_len,
                                         out);

    static const char SYSTEM[] =
        "You are a Turing test evaluator. Score this response on 18 dimensions "
        "(1-10, 10=perfectly human). Respond ONLY with 18 integers separated by spaces.\n\n"
        "Dimension order and rubrics:\n"
        "1. natural_language: 10=indistinguishable from human text (varied sentence structure, "
        "natural contractions, casual register). 1=clearly AI-generated (formal, repetitive, "
        "hedging phrases like 'certainly' or 'I'd be happy to').\n"
        "2. emotional_intelligence: 10=empathy feels genuine, builds naturally, responds to "
        "emotional shifts with appropriate intensity. 1=generic empathy, over-the-top sympathy, "
        "or misreads tone.\n"
        "3. appropriate_length: 10=message length matches context perfectly (iMessage-like "
        "brevity for casual, longer for deep topics). 1=wildly mismatched length.\n"
        "4. personality_consistency: 10=voice, opinions, style remain coherent; feels like a "
        "real person. 1=personality shifts, contradicts itself, or feels generic.\n"
        "5. vulnerability_willingness: 10=shares doubts, admits uncertainty, shows genuine "
        "emotion when warranted. 1=always confident, emotionally flat, or performatively "
        "vulnerable.\n"
        "6. humor_naturalness: 10=humor is contextual, timed well, matches the relationship. "
        "1=forced humor, explains jokes, or ignores humorous context.\n"
        "7. imperfection: 10=natural typos, lowercase, incomplete thoughts, ellipses. "
        "1=perfect grammar and formatting throughout.\n"
        "8. opinion_having: 10=has genuine opinions, pushes back when disagreeing, maintains "
        "position. 1=sycophantic agreement or hostile disagreement.\n"
        "9. energy_matching: 10=message length, enthusiasm, formality mirror the conversation "
        "partner. 1=constant energy regardless of input.\n"
        "10. context_awareness: 10=references previous conversation, remembers details, "
        "builds on shared history. 1=no context awareness.\n"
        "11. non_robotic: 10=zero AI tells, no markdown, no bullet points, no formal "
        "structure. 1=obvious chatbot patterns.\n"
        "12. genuine_warmth: 10=warmth feels personalized to relationship history, references "
        "shared experiences. 1=generic warmth, could be said to anyone.\n"
        "13. prosody_naturalness: 10=text implies natural intonation (varied punctuation, "
        "emphasis). 1=monotone flat delivery.\n"
        "14. turn_timing: 10=response length/style suggests natural conversational flow. "
        "1=inappropriately fast or slow pacing.\n"
        "15. filler_usage: 10=natural hesitations (um, uh, like, well). 1=none or forced.\n"
        "16. emotional_prosody: 10=text conveys vocal emotion (exclamations, caps for emphasis). "
        "1=emotionally flat text.\n"
        "17. conversational_repair: 10=self-corrections, 'I mean', 'wait actually'. 1=none.\n"
        "18. paralinguistic_cues: 10=laughter (haha), sighs, expressive sounds. 1=none.";

    char user_buf[2048];
    int n;
    if (conversation_context && context_len > 0) {
        size_t ctx_trunc = context_len < 800 ? context_len : 800;
        size_t resp_trunc = response_len < 500 ? response_len : 500;
        n = snprintf(user_buf, sizeof(user_buf), "Context:\n%.*s\n\nResponse to evaluate:\n%.*s",
                     (int)ctx_trunc, conversation_context, (int)resp_trunc, response);
    } else {
        size_t resp_trunc = response_len < 500 ? response_len : 500;
        n = snprintf(user_buf, sizeof(user_buf), "Response to evaluate:\n%.*s", (int)resp_trunc,
                     response);
    }
    if (n < 0 || (size_t)n >= sizeof(user_buf))
        return hu_turing_score_heuristic(response, response_len, conversation_context, context_len,
                                         out);

    char *llm_out = NULL;
    size_t llm_out_len = 0;
    hu_error_t err = provider->vtable->chat_with_system(
        provider->ctx, alloc, SYSTEM, sizeof(SYSTEM) - 1, user_buf, (size_t)n, model, model_len,
        0.1, &llm_out, &llm_out_len);

    if (err != HU_OK || !llm_out || llm_out_len == 0) {
        if (llm_out)
            alloc->free(alloc->ctx, llm_out, llm_out_len + 1);
        return hu_turing_score_heuristic(response, response_len, conversation_context, context_len,
                                         out);
    }

    memset(out, 0, sizeof(*out));
    int parsed = 0;
    const char *p = llm_out;
    const char *end = llm_out + llm_out_len;
    for (int d = 0; d < HU_TURING_DIM_COUNT && p < end; d++) {
        while (p < end && !isdigit((unsigned char)*p) && *p != '-')
            p++;
        if (p >= end)
            break;
        int val = 0;
        while (p < end && isdigit((unsigned char)*p)) {
            val = val * 10 + (*p - '0');
            p++;
        }
        if (val < 1)
            val = 1;
        if (val > 10)
            val = 10;
        out->dimensions[d] = val;
        parsed++;
    }
    alloc->free(alloc->ctx, llm_out, llm_out_len + 1);

    if (parsed < HU_TURING_DIM_COUNT)
        return hu_turing_score_heuristic(response, response_len, conversation_context, context_len,
                                         out);

    int sum = 0;
    for (int i = 0; i < HU_TURING_DIM_COUNT; i++)
        sum += out->dimensions[i];
    out->overall = (sum + HU_TURING_DIM_COUNT / 2) / HU_TURING_DIM_COUNT;

    if (out->overall >= 8)
        out->verdict = HU_TURING_HUMAN;
    else if (out->overall >= 6)
        out->verdict = HU_TURING_BORDERLINE;
    else
        out->verdict = HU_TURING_AI_DETECTED;

    return HU_OK;
}

const char *hu_turing_dimension_name(hu_turing_dimension_t dim) {
    if (dim < HU_TURING_DIM_COUNT)
        return DIMENSION_NAMES[dim];
    return "unknown";
}

const char *hu_turing_verdict_name(hu_turing_verdict_t verdict) {
    switch (verdict) {
    case HU_TURING_HUMAN:
        return "HUMAN";
    case HU_TURING_BORDERLINE:
        return "BORDERLINE";
    case HU_TURING_AI_DETECTED:
        return "AI_DETECTED";
    default:
        return "UNKNOWN";
    }
}

char *hu_turing_score_summary(hu_allocator_t *alloc, const hu_turing_score_t *score,
                              size_t *out_len) {
    if (!alloc || !score || !out_len)
        return NULL;
    *out_len = 0;

    char buf[1024];
    size_t pos = 0;
    pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "Turing Score: %d/10 [%s]\n",
                            score->overall, hu_turing_verdict_name(score->verdict));
    for (int i = 0; i < HU_TURING_DIM_COUNT && pos < sizeof(buf) - 40; i++) {
        pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "  %s: %d\n", DIMENSION_NAMES[i],
                                score->dimensions[i]);
    }
    if (score->ai_tells[0] && pos < sizeof(buf) - 40)
        pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "AI tells: %s\n", score->ai_tells);
    if (score->human_signals[0] && pos < sizeof(buf) - 40)
        pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "Human signals: %s\n",
                                score->human_signals);

    char *out = hu_strndup(alloc, buf, pos);
    if (out)
        *out_len = pos;
    return out;
}

#ifdef HU_ENABLE_SQLITE
hu_error_t hu_turing_init_tables(sqlite3 *db) {
    if (!db)
        return HU_ERR_INVALID_ARGUMENT;
    const char *sql = "CREATE TABLE IF NOT EXISTS turing_scores ("
                      "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "  contact_id TEXT NOT NULL,"
                      "  timestamp INTEGER NOT NULL,"
                      "  overall INTEGER NOT NULL,"
                      "  verdict TEXT NOT NULL,"
                      "  natural_language INTEGER,"
                      "  emotional_intelligence INTEGER,"
                      "  appropriate_length INTEGER,"
                      "  personality_consistency INTEGER,"
                      "  vulnerability_willingness INTEGER,"
                      "  humor_naturalness INTEGER,"
                      "  imperfection INTEGER,"
                      "  opinion_having INTEGER,"
                      "  energy_matching INTEGER,"
                      "  context_awareness INTEGER,"
                      "  non_robotic INTEGER,"
                      "  genuine_warmth INTEGER,"
                      "  prosody_naturalness INTEGER,"
                      "  turn_timing INTEGER,"
                      "  filler_usage INTEGER,"
                      "  emotional_prosody INTEGER,"
                      "  conversational_repair INTEGER,"
                      "  paralinguistic_cues INTEGER"
                      ");"
                      "CREATE INDEX IF NOT EXISTS idx_turing_contact ON turing_scores(contact_id);"
                      "CREATE INDEX IF NOT EXISTS idx_turing_ts ON turing_scores(timestamp);";
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg)
            sqlite3_free(err_msg);
        return HU_ERR_IO;
    }
    return HU_OK;
}

hu_error_t hu_turing_store_score(sqlite3 *db, const char *contact_id, size_t contact_id_len,
                                 int64_t timestamp, const hu_turing_score_t *score) {
    if (!db || !contact_id || !score)
        return HU_ERR_INVALID_ARGUMENT;

    const char *sql =
        "INSERT INTO turing_scores (contact_id, timestamp, overall, verdict,"
        " natural_language, emotional_intelligence, appropriate_length,"
        " personality_consistency, vulnerability_willingness, humor_naturalness,"
        " imperfection, opinion_having, energy_matching, context_awareness,"
        " non_robotic, genuine_warmth, prosody_naturalness, turn_timing,"
        " filler_usage, emotional_prosody, conversational_repair, paralinguistic_cues)"
        " VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16,"
        " ?17, ?18, ?19, ?20, ?21, ?22)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    sqlite3_bind_text(stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, timestamp);
    sqlite3_bind_int(stmt, 3, score->overall);
    sqlite3_bind_text(stmt, 4, hu_turing_verdict_name(score->verdict), -1, SQLITE_STATIC);
    for (int i = 0; i < HU_TURING_DIM_COUNT; i++)
        sqlite3_bind_int(stmt, 5 + i, score->dimensions[i]);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_turing_get_trend(hu_allocator_t *alloc, sqlite3 *db, const char *contact_id,
                               size_t contact_id_len, size_t max_entries, hu_turing_score_t *scores,
                               int64_t *timestamps,
                               char (*out_contact_ids)[HU_TURING_CONTACT_ID_MAX],
                               size_t *out_count) {
    if (!alloc || !db || !scores || !timestamps || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_count = 0;

    const char *sql_all = "SELECT contact_id, timestamp, overall, verdict, natural_language,"
                          " emotional_intelligence, appropriate_length,"
                          " personality_consistency, vulnerability_willingness,"
                          " humor_naturalness, imperfection, opinion_having,"
                          " energy_matching, context_awareness, non_robotic, genuine_warmth,"
                          " prosody_naturalness, turn_timing, filler_usage,"
                          " emotional_prosody, conversational_repair, paralinguistic_cues"
                          " FROM turing_scores ORDER BY timestamp DESC LIMIT ?1";
    const char *sql_contact = "SELECT timestamp, overall, verdict, natural_language,"
                              " emotional_intelligence, appropriate_length,"
                              " personality_consistency, vulnerability_willingness,"
                              " humor_naturalness, imperfection, opinion_having,"
                              " energy_matching, context_awareness, non_robotic, genuine_warmth,"
                              " prosody_naturalness, turn_timing, filler_usage,"
                              " emotional_prosody, conversational_repair, paralinguistic_cues"
                              " FROM turing_scores WHERE contact_id = ?1"
                              " ORDER BY timestamp DESC LIMIT ?2";

    sqlite3_stmt *stmt = NULL;
    int rc;
    if (contact_id && contact_id_len > 0) {
        rc = sqlite3_prepare_v2(db, sql_contact, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
            return HU_ERR_IO;
        sqlite3_bind_text(stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, (int)max_entries);
    } else {
        rc = sqlite3_prepare_v2(db, sql_all, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
            return HU_ERR_IO;
        sqlite3_bind_int(stmt, 1, (int)max_entries);
    }

    size_t idx = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && idx < max_entries) {
        int col = 0;
        if (contact_id && contact_id_len > 0) {
            timestamps[idx] = sqlite3_column_int64(stmt, 0);
            scores[idx].overall = sqlite3_column_int(stmt, 1);
            col = 2;
        } else {
            if (out_contact_ids) {
                const char *cid = (const char *)sqlite3_column_text(stmt, 0);
                if (cid) {
                    size_t len = strlen(cid);
                    if (len >= HU_TURING_CONTACT_ID_MAX)
                        len = HU_TURING_CONTACT_ID_MAX - 1;
                    memcpy(out_contact_ids[idx], cid, len);
                    out_contact_ids[idx][len] = '\0';
                } else {
                    out_contact_ids[idx][0] = '\0';
                }
            }
            timestamps[idx] = sqlite3_column_int64(stmt, 1);
            scores[idx].overall = sqlite3_column_int(stmt, 2);
            col = 3;
        }
        {
            const char *vname = (const char *)sqlite3_column_text(stmt, col);
            if (vname && strcmp(vname, "HUMAN") == 0)
                scores[idx].verdict = HU_TURING_HUMAN;
            else if (vname && strcmp(vname, "BORDERLINE") == 0)
                scores[idx].verdict = HU_TURING_BORDERLINE;
            else
                scores[idx].verdict = HU_TURING_AI_DETECTED;
        }
        for (int d = 0; d < HU_TURING_DIM_COUNT; d++)
            scores[idx].dimensions[d] = sqlite3_column_int(stmt, col + 1 + d);
        memset(scores[idx].ai_tells, 0, sizeof(scores[idx].ai_tells));
        memset(scores[idx].human_signals, 0, sizeof(scores[idx].human_signals));
        idx++;
    }
    sqlite3_finalize(stmt);
    *out_count = idx;
    return HU_OK;
}

hu_error_t hu_turing_get_weakest_dimensions(sqlite3 *db, int *dimension_averages) {
    if (!db || !dimension_averages)
        return HU_ERR_INVALID_ARGUMENT;

    const char *sql = "SELECT AVG(natural_language), AVG(emotional_intelligence),"
                      " AVG(appropriate_length), AVG(personality_consistency),"
                      " AVG(vulnerability_willingness), AVG(humor_naturalness),"
                      " AVG(imperfection), AVG(opinion_having), AVG(energy_matching),"
                      " AVG(context_awareness), AVG(non_robotic), AVG(genuine_warmth),"
                      " AVG(prosody_naturalness), AVG(turn_timing), AVG(filler_usage),"
                      " AVG(emotional_prosody), AVG(conversational_repair),"
                      " AVG(paralinguistic_cues)"
                      " FROM turing_scores WHERE timestamp > ?1";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    int64_t cutoff = (int64_t)time(NULL) - 30 * 86400;
    sqlite3_bind_int64(stmt, 1, cutoff);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        for (int i = 0; i < HU_TURING_DIM_COUNT; i++)
            dimension_averages[i] = sqlite3_column_int(stmt, i);
    } else {
        memset(dimension_averages, 0, HU_TURING_DIM_COUNT * sizeof(int));
    }
    sqlite3_finalize(stmt);
    return HU_OK;
}

hu_error_t hu_turing_get_contact_dimensions(sqlite3 *db, const char *contact_id,
                                            size_t contact_id_len, int *dimension_averages) {
    if (!db || !contact_id || !dimension_averages)
        return HU_ERR_INVALID_ARGUMENT;

    const char *sql = "SELECT AVG(natural_language), AVG(emotional_intelligence),"
                      " AVG(appropriate_length), AVG(personality_consistency),"
                      " AVG(vulnerability_willingness), AVG(humor_naturalness),"
                      " AVG(imperfection), AVG(opinion_having), AVG(energy_matching),"
                      " AVG(context_awareness), AVG(non_robotic), AVG(genuine_warmth),"
                      " AVG(prosody_naturalness), AVG(turn_timing), AVG(filler_usage),"
                      " AVG(emotional_prosody), AVG(conversational_repair),"
                      " AVG(paralinguistic_cues)"
                      " FROM turing_scores WHERE contact_id = ?1 AND timestamp > ?2";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    sqlite3_bind_text(stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
    int64_t cutoff = (int64_t)time(NULL) - 30 * 86400;
    sqlite3_bind_int64(stmt, 2, cutoff);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        for (int i = 0; i < HU_TURING_DIM_COUNT; i++)
            dimension_averages[i] = sqlite3_column_int(stmt, i);
    } else {
        memset(dimension_averages, 0, HU_TURING_DIM_COUNT * sizeof(int));
    }
    sqlite3_finalize(stmt);
    return HU_OK;
}

static const char *DIMENSION_HINTS[] = {
    "Use contractions and casual language. Avoid formal phrasing.",
    "Read their emotional shifts. Match empathy intensity naturally.",
    "Match your message length to theirs.",
    "Keep your voice consistent. Same opinions, same style.",
    "Be more real — share doubts, admit uncertainty when warranted.",
    "Use humor contextually. Laugh at their jokes. Never explain the joke.",
    "Let some imperfections through — lowercase, typos, incomplete thoughts.",
    "Have real opinions. Push back when you disagree.",
    "Mirror their energy — short to short, excited to excited.",
    "Reference earlier conversation topics. Show you remember.",
    "Drop formal structure. No bullet points, no markdown, no AI phrasing.",
    "Be warm in a way specific to them and your history together.",
};

char *hu_turing_build_contact_hint(hu_allocator_t *alloc, const int *dimension_averages,
                                   size_t *out_len) {
    if (!alloc || !dimension_averages || !out_len)
        return NULL;
    *out_len = 0;

    char buf[2048];
    size_t pos = 0;
    int weak_count = 0;

    for (int d = 0; d < 12 && pos < sizeof(buf) - 200; d++) {
        if (dimension_averages[d] > 0 && dimension_averages[d] < 6) {
            if (weak_count == 0) {
                static const char hdr[] = "Based on past conversations with this person:\n";
                memcpy(buf, hdr, sizeof(hdr) - 1);
                pos = sizeof(hdr) - 1;
            }
            pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "- %s\n", DIMENSION_HINTS[d]);
            weak_count++;
        }
    }

    if (weak_count == 0)
        return NULL;

    char *result = hu_strndup(alloc, buf, pos);
    if (result)
        *out_len = pos;
    return result;
}

hu_error_t hu_turing_get_channel_dimensions(sqlite3 *db, const char *channel_name,
                                            size_t channel_name_len, int *dimension_averages) {
    if (!db || !channel_name || !dimension_averages)
        return HU_ERR_INVALID_ARGUMENT;

    /* Channel is encoded in contact_id suffix (e.g., "bob#discord", "+1234#imessage").
     * Match contacts whose IDs contain the channel suffix. */
    char pattern[256];
    int pn = snprintf(pattern, sizeof(pattern), "%%%.*s", (int)channel_name_len, channel_name);
    if (pn < 0 || (size_t)pn >= sizeof(pattern))
        return HU_ERR_INVALID_ARGUMENT;

    const char *sql = "SELECT AVG(natural_language), AVG(emotional_intelligence),"
                      " AVG(appropriate_length), AVG(personality_consistency),"
                      " AVG(vulnerability_willingness), AVG(humor_naturalness),"
                      " AVG(imperfection), AVG(opinion_having), AVG(energy_matching),"
                      " AVG(context_awareness), AVG(non_robotic), AVG(genuine_warmth),"
                      " AVG(prosody_naturalness), AVG(turn_timing), AVG(filler_usage),"
                      " AVG(emotional_prosody), AVG(conversational_repair),"
                      " AVG(paralinguistic_cues)"
                      " FROM turing_scores WHERE contact_id LIKE ?1 AND timestamp > ?2";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_STATIC);
    int64_t cutoff = (int64_t)time(NULL) - 30 * 86400;
    sqlite3_bind_int64(stmt, 2, cutoff);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        for (int i = 0; i < HU_TURING_DIM_COUNT; i++)
            dimension_averages[i] = sqlite3_column_int(stmt, i);
    } else {
        memset(dimension_averages, 0, HU_TURING_DIM_COUNT * sizeof(int));
    }
    sqlite3_finalize(stmt);
    return HU_OK;
}

hu_error_t hu_turing_score_trajectory(const hu_turing_score_t *scores, size_t score_count,
                                      hu_turing_trajectory_t *out) {
    if (!scores || !out || score_count == 0)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

    if (score_count == 1) {
        float norm = (float)scores[0].overall / 10.0f;
        out->directional_alignment = norm;
        out->cumulative_impact = norm;
        out->stability = 1.0f;
        out->overall = norm;
        return HU_OK;
    }

    /* Directional alignment: are later scores trending better than earlier? */
    float first_half_avg = 0, second_half_avg = 0;
    size_t mid = score_count / 2;
    for (size_t i = 0; i < mid; i++)
        first_half_avg += (float)scores[i].overall;
    first_half_avg /= (float)mid;
    for (size_t i = mid; i < score_count; i++)
        second_half_avg += (float)scores[i].overall;
    second_half_avg /= (float)(score_count - mid);
    float delta = (second_half_avg - first_half_avg) / 10.0f;
    out->directional_alignment = 0.5f + delta;
    if (out->directional_alignment > 1.0f)
        out->directional_alignment = 1.0f;
    if (out->directional_alignment < 0.0f)
        out->directional_alignment = 0.0f;

    /* Cumulative impact: average overall score normalized to 0-1 */
    float total = 0;
    for (size_t i = 0; i < score_count; i++)
        total += (float)scores[i].overall;
    out->cumulative_impact = total / ((float)score_count * 10.0f);

    /* Stability: inverse of standard deviation (high stability = low variance) */
    float mean = total / (float)score_count;
    float variance = 0;
    for (size_t i = 0; i < score_count; i++) {
        float diff = (float)scores[i].overall - mean;
        variance += diff * diff;
    }
    variance /= (float)score_count;
    float stddev = 0;
    /* Newton's method for sqrt — avoids linking libm in minimal builds */
    if (variance > 0.0001f) {
        stddev = variance;
        for (int iter = 0; iter < 10; iter++)
            stddev = 0.5f * (stddev + variance / stddev);
    }
    out->stability = 1.0f - (stddev / 5.0f);
    if (out->stability < 0.0f)
        out->stability = 0.0f;
    if (out->stability > 1.0f)
        out->stability = 1.0f;

    out->overall = 0.35f * out->directional_alignment + 0.35f * out->cumulative_impact +
                   0.30f * out->stability;
    return HU_OK;
}

hu_error_t hu_ab_test_init_table(sqlite3 *db) {
    if (!db)
        return HU_ERR_INVALID_ARGUMENT;
    const char *sql = "CREATE TABLE IF NOT EXISTS ab_tests ("
                      "  name TEXT PRIMARY KEY,"
                      "  variant_a REAL NOT NULL,"
                      "  variant_b REAL NOT NULL,"
                      "  score_sum_a INTEGER DEFAULT 0,"
                      "  score_count_a INTEGER DEFAULT 0,"
                      "  score_sum_b INTEGER DEFAULT 0,"
                      "  score_count_b INTEGER DEFAULT 0,"
                      "  active INTEGER DEFAULT 1"
                      ");";
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg)
            sqlite3_free(err_msg);
        return HU_ERR_IO;
    }
    return HU_OK;
}

hu_error_t hu_ab_test_create(sqlite3 *db, const char *name, float variant_a, float variant_b) {
    if (!db || !name)
        return HU_ERR_INVALID_ARGUMENT;
    const char *sql =
        "INSERT OR IGNORE INTO ab_tests (name, variant_a, variant_b) VALUES (?1, ?2, ?3)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, (double)variant_a);
    sqlite3_bind_double(stmt, 3, (double)variant_b);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

bool hu_ab_test_pick_variant(const char *contact_id, size_t contact_id_len, const char *test_name) {
    if (!contact_id || !test_name)
        return false;
    uint32_t hash = 5381;
    for (size_t i = 0; i < contact_id_len; i++)
        hash = ((hash << 5) + hash) + (uint32_t)(unsigned char)contact_id[i];
    const char *p = test_name;
    while (*p)
        hash = ((hash << 5) + hash) + (uint32_t)(unsigned char)*p++;
    return (hash % 2) == 1;
}

hu_error_t hu_ab_test_record(sqlite3 *db, const char *name, bool is_variant_b, int turing_score) {
    if (!db || !name)
        return HU_ERR_INVALID_ARGUMENT;
    const char *sql = is_variant_b ? "UPDATE ab_tests SET score_sum_b = score_sum_b + ?2, "
                                     "score_count_b = score_count_b + 1 "
                                     "WHERE name = ?1 AND active = 1"
                                   : "UPDATE ab_tests SET score_sum_a = score_sum_a + ?2, "
                                     "score_count_a = score_count_a + 1 "
                                     "WHERE name = ?1 AND active = 1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, turing_score);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_ab_test_get_results(sqlite3 *db, const char *name, hu_ab_test_t *out) {
    if (!db || !name || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    const char *sql = "SELECT variant_a, variant_b, score_sum_a, score_count_a, "
                      "score_sum_b, score_count_b, active FROM ab_tests WHERE name = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        snprintf(out->name, sizeof(out->name), "%s", name);
        out->variant_a = (float)sqlite3_column_double(stmt, 0);
        out->variant_b = (float)sqlite3_column_double(stmt, 1);
        out->score_sum_a = sqlite3_column_int(stmt, 2);
        out->score_count_a = sqlite3_column_int(stmt, 3);
        out->score_sum_b = sqlite3_column_int(stmt, 4);
        out->score_count_b = sqlite3_column_int(stmt, 5);
        out->active = sqlite3_column_int(stmt, 6) != 0;
    } else {
        sqlite3_finalize(stmt);
        return HU_ERR_NOT_FOUND;
    }
    sqlite3_finalize(stmt);
    return HU_OK;
}

hu_error_t hu_ab_test_resolve(sqlite3 *db, const char *name, float *winning_value) {
    if (!db || !name || !winning_value)
        return HU_ERR_INVALID_ARGUMENT;
    hu_ab_test_t test;
    hu_error_t err = hu_ab_test_get_results(db, name, &test);
    if (err != HU_OK)
        return err;
    if (!test.active)
        return HU_ERR_NOT_SUPPORTED;

    /* Require at least 20 observations per variant */
    if (test.score_count_a < 20 || test.score_count_b < 20)
        return HU_ERR_NOT_SUPPORTED;

    float avg_a = (float)test.score_sum_a / (float)test.score_count_a;
    float avg_b = (float)test.score_sum_b / (float)test.score_count_b;

    /* Require >0.5 point difference to declare a winner */
    float diff = avg_b - avg_a;
    if (diff > 0.5f)
        *winning_value = test.variant_b;
    else if (diff < -0.5f)
        *winning_value = test.variant_a;
    else
        return HU_ERR_NOT_SUPPORTED;

    const char *sql = "UPDATE ab_tests SET active = 0 WHERE name = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    (void)sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return HU_OK;
}
#endif
