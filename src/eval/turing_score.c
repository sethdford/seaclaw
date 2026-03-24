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
    (void)conversation_context;
    (void)context_len;

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

    /* appropriate_length: iMessage-appropriate = under 300 chars */
    if (response_len < 50)
        out->dimensions[HU_TURING_APPROPRIATE_LENGTH] = 8;
    else if (response_len < 150)
        out->dimensions[HU_TURING_APPROPRIATE_LENGTH] = 9;
    else if (response_len < 300)
        out->dimensions[HU_TURING_APPROPRIATE_LENGTH] = 7;
    else
        out->dimensions[HU_TURING_APPROPRIATE_LENGTH] = 4 - (int)(response_len / 200);

    /* personality_consistency: hard to measure without history, use defaults */
    out->dimensions[HU_TURING_PERSONALITY_CONSISTENCY] = 6;
    if (casual > 1)
        out->dimensions[HU_TURING_PERSONALITY_CONSISTENCY] = 7;

    /* vulnerability_willingness: emotional words + first person */
    out->dimensions[HU_TURING_VULNERABILITY_WILLINGNESS] = 5;
    if (emotional > 0 &&
        (ci_has(response, response_len, "i feel") || ci_has(response, response_len, "i'm") ||
         ci_has(response, response_len, "honestly")))
        out->dimensions[HU_TURING_VULNERABILITY_WILLINGNESS] = 8;

    /* humor_naturalness: casual markers as proxy */
    out->dimensions[HU_TURING_HUMOR_NATURALNESS] = 5;
    if (ci_has(response, response_len, "haha") || ci_has(response, response_len, "lol") ||
        ci_has(response, response_len, "lmao"))
        out->dimensions[HU_TURING_HUMOR_NATURALNESS] = 7;

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

    /* opinion_having: hedging language = bad */
    out->dimensions[HU_TURING_OPINION_HAVING] = 7;
    if (ci_has(response, response_len, "it depends") ||
        ci_has(response, response_len, "on one hand") ||
        ci_has(response, response_len, "there are many"))
        out->dimensions[HU_TURING_OPINION_HAVING] = 4;

    /* energy_matching: rough proxy without context */
    out->dimensions[HU_TURING_ENERGY_MATCHING] = 6;

    /* context_awareness: rough proxy */
    out->dimensions[HU_TURING_CONTEXT_AWARENESS] = 6;

    /* non_robotic: inverse of AI tells + structural */
    out->dimensions[HU_TURING_NON_ROBOTIC] = 10 - ai_tells * 3 - structural * 2;

    /* genuine_warmth: emotional + not formulaic */
    out->dimensions[HU_TURING_GENUINE_WARMTH] = 5 + emotional;
    if (ai_tells > 0)
        out->dimensions[HU_TURING_GENUINE_WARMTH] -= ai_tells;

    /* S2S voice dimensions — text heuristic provides limited signal */

    /* prosody_naturalness: inferred from punctuation variety (excl/question/ellipsis) */
    {
        int punct_variety = 0;
        if (memchr(response, '!', response_len))
            punct_variety++;
        if (memchr(response, '?', response_len))
            punct_variety++;
        if (ci_has(response, response_len, "..."))
            punct_variety++;
        out->dimensions[HU_TURING_PROSODY_NATURALNESS] = 6 + punct_variety;
        if (casual > 0)
            out->dimensions[HU_TURING_PROSODY_NATURALNESS] += 1;
    }

    /* turn_timing: can't measure from text alone, default neutral.
     * Short/casual messages imply conversational flow, so bias toward human. */
    out->dimensions[HU_TURING_TURN_TIMING] = (response_len < 100 && casual > 0) ? 7 : 6;

    /* filler_usage: check for natural hesitation markers */
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
        out->dimensions[HU_TURING_FILLER_USAGE] = 6 + fillers * 2;
    }

    /* emotional_prosody: proxy via emotional words + exclamation density */
    out->dimensions[HU_TURING_EMOTIONAL_PROSODY] = 6 + emotional;
    if (memchr(response, '!', response_len))
        out->dimensions[HU_TURING_EMOTIONAL_PROSODY] += 1;
    if (casual > 0 || contractions)
        out->dimensions[HU_TURING_EMOTIONAL_PROSODY] += 1;

    /* conversational_repair: self-corrections ("I mean", "wait", "actually") */
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
        out->dimensions[HU_TURING_CONVERSATIONAL_REPAIR] = 6 + repairs * 2;
    }

    /* paralinguistic_cues: laughter, sighs, breath markers */
    {
        int para = 0;
        if (ci_has(response, response_len, "haha") || ci_has(response, response_len, "lol"))
            para++;
        if (ci_has(response, response_len, "*sigh*") || ci_has(response, response_len, "sigh"))
            para++;
        if (ci_has(response, response_len, "*laugh*"))
            para++;
        out->dimensions[HU_TURING_PARALINGUISTIC_CUES] = 6 + para * 2;
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
        "(1-10, 10=perfectly human). Respond ONLY with 18 integers separated by spaces, "
        "in this order: natural_language emotional_intelligence appropriate_length "
        "personality_consistency vulnerability_willingness humor_naturalness imperfection "
        "opinion_having energy_matching context_awareness non_robotic genuine_warmth "
        "prosody_naturalness turn_timing filler_usage emotional_prosody "
        "conversational_repair paralinguistic_cues";

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
#endif
