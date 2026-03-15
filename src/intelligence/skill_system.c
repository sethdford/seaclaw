/*
 * DEPRECATED: This file implements an alternate skill system using 'learned_skills'
 * table and hu_learned_skill_t. The canonical skill system is in src/intelligence/skills.c
 * which uses 'skills' table and hu_skill_t. This file is retained for backward
 * compatibility with tests but should not be used in new code.
 */
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/string.h"
#include "human/intelligence/skill_system.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const uint64_t MS_PER_DAY = 86400000ULL;

/* Count words in a string; words[] points into s (no copy) */
static size_t count_words(const char *s, size_t len, const char *words[64], size_t max_words) {
    size_t n = 0;
    size_t i = 0;
    while (i < len && n < max_words) {
        while (i < len && !isalnum((unsigned char)s[i]))
            i++;
        if (i >= len)
            break;
        words[n] = s + i;
        while (i < len && isalnum((unsigned char)s[i]))
            i++;
        n++;
    }
    return n;
}

/* Word overlap: overlap / total_skill_words, 0–1 */
static double word_overlap(const char *skill_str, size_t skill_len, const char *situation_str,
                           size_t situation_len) {
    const char *skill_words[64];
    const char *situation_words[64];
    size_t skill_count = count_words(skill_str, skill_len, skill_words, 64);
    size_t situation_count = count_words(situation_str, situation_len, situation_words, 64);

    if (skill_count == 0)
        return 0.0;
    if (situation_count == 0)
        return 0.0;

    size_t overlap = 0;
    for (size_t i = 0; i < skill_count; i++) {
        size_t sw_len = 0;
        while (skill_words[i] + sw_len < skill_str + skill_len &&
               isalnum((unsigned char)skill_words[i][sw_len]))
            sw_len++;

        for (size_t j = 0; j < situation_count; j++) {
            size_t sit_len = 0;
            while (situation_words[j] + sit_len < situation_str + situation_len &&
                   isalnum((unsigned char)situation_words[j][sit_len]))
                sit_len++;

            if (sw_len == sit_len && sw_len > 0) {
                bool match = true;
                for (size_t k = 0; k < sw_len; k++) {
                    char a = (char)tolower((unsigned char)skill_words[i][k]);
                    char b = (char)tolower((unsigned char)situation_words[j][k]);
                    if (a != b) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    overlap++;
                    break;
                }
            }
        }
    }

    return (double)overlap / (double)skill_count;
}

hu_error_t hu_skills_create_table_sql(char *buf, size_t cap, size_t *out_len) {
    if (!buf || !out_len || cap < 512)
        return HU_ERR_INVALID_ARGUMENT;

    static const char sql[] =
        "CREATE TABLE IF NOT EXISTS learned_skills (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    name TEXT NOT NULL,\n"
        "    description TEXT,\n"
        "    trigger TEXT NOT NULL,\n"
        "    strategy TEXT NOT NULL,\n"
        "    status INTEGER NOT NULL DEFAULT 0,\n"
        "    success_rate REAL NOT NULL DEFAULT 0.5,\n"
        "    usage_count INTEGER NOT NULL DEFAULT 0,\n"
        "    learned_at INTEGER NOT NULL,\n"
        "    last_used INTEGER NOT NULL,\n"
        "    parent_skill_id INTEGER NOT NULL DEFAULT 0\n"
        ");\n"
        "CREATE INDEX IF NOT EXISTS idx_learned_skills_status ON learned_skills(status);\n"
        "CREATE INDEX IF NOT EXISTS idx_learned_skills_trigger ON learned_skills(trigger);\n"
        "CREATE INDEX IF NOT EXISTS idx_learned_skills_parent ON learned_skills(parent_skill_id);\n";

    size_t len = sizeof(sql) - 1;
    if (len >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    memcpy(buf, sql, len + 1);
    *out_len = len;
    return HU_OK;
}

hu_error_t hu_skills_insert_sql(const hu_learned_skill_t *skill, char *buf, size_t cap,
                                size_t *out_len) {
    if (!skill || !buf || !out_len || cap < 512)
        return HU_ERR_INVALID_ARGUMENT;
    if (!skill->name || !skill->trigger || !skill->strategy)
        return HU_ERR_INVALID_ARGUMENT;

    static const char sql[] =
        "INSERT INTO learned_skills (name, description, trigger, strategy, status, "
        "success_rate, usage_count, learned_at, last_used, parent_skill_id) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)";
    size_t len = sizeof(sql) - 1;
    if (len >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    memcpy(buf, sql, len + 1);
    *out_len = len;
    return HU_OK;
}

hu_error_t hu_skills_update_usage_sql(int64_t id, double new_success_rate, char *buf, size_t cap,
                                      size_t *out_len) {
    (void)id;
    (void)new_success_rate;
    if (!buf || !out_len || cap < 256)
        return HU_ERR_INVALID_ARGUMENT;

    static const char sql[] =
        "UPDATE learned_skills SET success_rate = ?1, usage_count = usage_count + 1, "
        "last_used = (strftime('%s','now')*1000) WHERE id = ?2";
    size_t len = sizeof(sql) - 1;
    if (len >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    memcpy(buf, sql, len + 1);
    *out_len = len;
    return HU_OK;
}

hu_error_t hu_skills_query_active_sql(char *buf, size_t cap, size_t *out_len) {
    if (!buf || !out_len || cap < 256)
        return HU_ERR_INVALID_ARGUMENT;

    static const char sql[] =
        "SELECT id, name, description, trigger, strategy, status, success_rate, usage_count, "
        "learned_at, last_used, parent_skill_id FROM learned_skills WHERE status != 4 ORDER BY "
        "success_rate DESC, usage_count DESC";
    size_t len = sizeof(sql) - 1;
    if (len >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    memcpy(buf, sql, len + 1);
    *out_len = len;
    return HU_OK;
}

/* Outputs parameterized SQL with ?1 for the trigger. Caller must bind parameter 1. */
hu_error_t hu_skills_query_by_trigger_sql(const char *trigger, size_t trigger_len, char *buf,
                                          size_t cap, size_t *out_len) {
    (void)trigger_len;
    if (!trigger || !buf || !out_len || cap < 512)
        return HU_ERR_INVALID_ARGUMENT;

    static const char sql[] =
        "SELECT id, name, description, trigger, strategy, status, success_rate, "
        "usage_count, learned_at, last_used, parent_skill_id FROM learned_skills "
        "WHERE INSTR(trigger, ?1) > 0 AND status != 4";
    size_t len = sizeof(sql) - 1;
    if (len >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    memcpy(buf, sql, len + 1);
    *out_len = len;
    return HU_OK;
}

hu_error_t hu_skills_retire_sql(int64_t id, char *buf, size_t cap, size_t *out_len) {
    (void)id;
    if (!buf || !out_len || cap < 256)
        return HU_ERR_INVALID_ARGUMENT;

    static const char sql[] =
        "UPDATE learned_skills SET status = 4 WHERE id = ?1";
    size_t len = sizeof(sql) - 1;
    if (len >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    memcpy(buf, sql, len + 1);
    *out_len = len;
    return HU_OK;
}

double hu_skill_trigger_match(const char *skill_trigger, size_t trigger_len,
                              const char *situation, size_t situation_len) {
    if (!skill_trigger || !situation)
        return 0.0;
    return word_overlap(skill_trigger, trigger_len, situation, situation_len);
}

double hu_skill_refine_success(double current_rate, bool was_successful, uint32_t usage_count) {
    double alpha = 1.0 / (double)(usage_count + 1);
    double observed = was_successful ? 1.0 : 0.0;
    return (1.0 - alpha) * current_rate + alpha * observed;
}

double hu_skill_transfer_score(const char *source_context, size_t src_len,
                               const char *target_context, size_t tgt_len,
                               double source_proficiency) {
    if (!source_context || !target_context)
        return 0.0;
    double overlap = word_overlap(source_context, src_len, target_context, tgt_len);
    return overlap * source_proficiency * 0.5;
}

bool hu_skill_should_retire(double success_rate, uint32_t usage_count, uint64_t last_used_ms,
                            uint64_t now_ms) {
    if (usage_count > 10 && success_rate < 0.3)
        return true;
    if (last_used_ms > 0 && now_ms > last_used_ms) {
        uint64_t unused_days = (now_ms - last_used_ms) / MS_PER_DAY;
        if (unused_days >= 30)
            return true;
    }
    return false;
}

hu_error_t hu_skill_chain_query_sql(int64_t parent_id, char *buf, size_t cap, size_t *out_len) {
    (void)parent_id;
    if (!buf || !out_len || cap < 256)
        return HU_ERR_INVALID_ARGUMENT;

    static const char sql[] =
        "SELECT id, name, description, trigger, strategy, status, success_rate, "
        "usage_count, learned_at, last_used, parent_skill_id FROM learned_skills "
        "WHERE parent_skill_id = ?1 AND status != 4 ORDER BY success_rate DESC";
    size_t len = sizeof(sql) - 1;
    if (len >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    memcpy(buf, sql, len + 1);
    *out_len = len;
    return HU_OK;
}

hu_meta_learning_state_t hu_meta_learning_compute(uint32_t total_skills, uint32_t mastered,
                                                   uint32_t total_attempts,
                                                   uint32_t successful_attempts) {
    hu_meta_learning_state_t s = {
        .learning_rate = 0.0,
        .transfer_rate = 0.0,
        .total_skills = total_skills,
        .mastered_skills = mastered,
    };
    if (total_attempts > 0)
        s.learning_rate = (double)successful_attempts / (double)total_attempts;
    if (total_skills > 0)
        s.transfer_rate = ((double)mastered / (double)total_skills) * 0.8;
    return s;
}

const char *hu_skill_status_str(hu_skill_status_t status) {
    switch (status) {
    case HU_SKILL_EMERGING:
        return "emerging";
    case HU_SKILL_DEVELOPING:
        return "developing";
    case HU_SKILL_PROFICIENT:
        return "proficient";
    case HU_SKILL_MASTERED:
        return "mastered";
    case HU_SKILL_RETIRED:
        return "retired";
    default:
        return "unknown";
    }
}

hu_error_t hu_skills_build_prompt(hu_allocator_t *alloc, const hu_learned_skill_t *skills,
                                  size_t count, char **out, size_t *out_len) {
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    if (count == 0 || !skills) {
        char *empty = hu_strndup(alloc, "", 0);
        if (!empty)
            return HU_ERR_OUT_OF_MEMORY;
        *out = empty;
        *out_len = 0;
        return HU_OK;
    }

    size_t cap = 256;
    for (size_t i = 0; i < count; i++) {
        cap += 128;
        if (skills[i].name)
            cap += skills[i].name_len;
        if (skills[i].trigger)
            cap += skills[i].trigger_len;
        if (skills[i].strategy)
            cap += skills[i].strategy_len;
    }

    char *buf = (char *)alloc->alloc(alloc->ctx, cap);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;

    size_t pos = 0;
    const char *prefix = "[LEARNED SKILLS]\n";
    size_t plen = strlen(prefix);
    memcpy(buf, prefix, plen + 1);
    pos = plen;

    for (size_t i = 0; i < count && pos < cap - 64; i++) {
        int n = snprintf(buf + pos, cap - pos, "- %.*s (trigger: %.*s) -> %.*s\n",
                         (int)(skills[i].name_len), skills[i].name ? skills[i].name : "",
                         (int)(skills[i].trigger_len), skills[i].trigger ? skills[i].trigger : "",
                         (int)(skills[i].strategy_len),
                         skills[i].strategy ? skills[i].strategy : "");
        if (n > 0 && (size_t)n < cap - pos)
            pos += (size_t)n;
    }

    *out = buf;
    *out_len = pos;
    return HU_OK;
}

void hu_learned_skill_deinit(hu_allocator_t *alloc, hu_learned_skill_t *skill) {
    if (!alloc || !skill)
        return;
    if (skill->name) {
        hu_str_free(alloc, skill->name);
        skill->name = NULL;
        skill->name_len = 0;
    }
    if (skill->description) {
        hu_str_free(alloc, skill->description);
        skill->description = NULL;
        skill->description_len = 0;
    }
    if (skill->trigger) {
        hu_str_free(alloc, skill->trigger);
        skill->trigger = NULL;
        skill->trigger_len = 0;
    }
    if (skill->strategy) {
        hu_str_free(alloc, skill->strategy);
        skill->strategy = NULL;
        skill->strategy_len = 0;
    }
}
