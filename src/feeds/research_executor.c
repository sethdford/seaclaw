#include "human/feeds/research_executor.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <string.h>
#include <time.h>
#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#endif

/* Case-insensitive substring check. */
static bool contains_word(const char *haystack, size_t hlen, const char *needle, size_t nlen) {
    if (!haystack || !needle || nlen == 0 || hlen < nlen)
        return false;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            char a = (char)((unsigned char)haystack[i + j] | 32);
            char b = (char)((unsigned char)needle[j] | 32);
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

hu_error_t hu_research_classify_action(const char *suggested, size_t suggested_len,
                                        hu_research_action_t *action) {
    if (!suggested || !action)
        return HU_ERR_INVALID_ARGUMENT;

    memset(action, 0, sizeof(*action));
    action->type = HU_RESEARCH_ACTION_KNOWLEDGE_ADD; /* default */
    action->is_safe = true;
    action->executed = false;
    action->executed_at = 0;

    size_t copy_len = suggested_len < sizeof(action->description) - 1
                         ? suggested_len
                         : sizeof(action->description) - 1;
    memcpy(action->description, suggested, copy_len);
    action->description[copy_len] = '\0';
    action->description_len = copy_len;

    if (contains_word(suggested, suggested_len, "prompt", 6)) {
        action->type = HU_RESEARCH_ACTION_PROMPT_UPDATE;
        action->is_safe = true;
        return HU_OK;
    }
    if (contains_word(suggested, suggested_len, "skill", 5)) {
        action->type = HU_RESEARCH_ACTION_SKILL_CREATE;
        action->is_safe = false;
        return HU_OK;
    }
    if (contains_word(suggested, suggested_len, "knowledge", 9) ||
        contains_word(suggested, suggested_len, "learn", 5)) {
        action->type = HU_RESEARCH_ACTION_KNOWLEDGE_ADD;
        action->is_safe = true;
        return HU_OK;
    }
    if (contains_word(suggested, suggested_len, "config", 6) ||
        contains_word(suggested, suggested_len, "setting", 7)) {
        action->type = HU_RESEARCH_ACTION_CONFIG_SUGGEST;
        action->is_safe = false;
        return HU_OK;
    }

    return HU_OK;
}

#ifdef HU_ENABLE_SQLITE

hu_error_t hu_research_execute_safe(hu_allocator_t *alloc, sqlite3 *db,
                                     const hu_research_action_t *action) {
    (void)alloc;
    if (!action)
        return HU_ERR_INVALID_ARGUMENT;
    if (!action->is_safe)
        return HU_ERR_SECURITY_APPROVAL_REQUIRED;

#ifdef HU_IS_TEST
    (void)db;
    /* In test mode, just mark as executed. */
    ((hu_research_action_t *)action)->executed = true;
    ((hu_research_action_t *)action)->executed_at = (int64_t)time(NULL);
    return HU_OK;
#else
    if (!db)
        return HU_ERR_INVALID_ARGUMENT;

    int64_t now = (int64_t)time(NULL);
    ((hu_research_action_t *)action)->executed = true;
    ((hu_research_action_t *)action)->executed_at = now;

    if (action->type == HU_RESEARCH_ACTION_KNOWLEDGE_ADD) {
        sqlite3_stmt *stmt = NULL;
        int rc = sqlite3_prepare_v2(db,
            "INSERT INTO general_lessons (lesson, confidence, source_count, first_learned, last_confirmed) "
            "VALUES (?, 0.5, 1, ?, ?)",
            -1, &stmt, NULL);
        if (rc != SQLITE_OK)
            return HU_ERR_MEMORY_BACKEND;
        sqlite3_bind_text(stmt, 1, action->description, (int)action->description_len, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, now);
        sqlite3_bind_int64(stmt, 3, now);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE)
            return HU_ERR_MEMORY_BACKEND;
        return HU_OK;
    }
    if (action->type == HU_RESEARCH_ACTION_PROMPT_UPDATE) {
        /* Log the update; no persistent storage required for prompt patches. */
        (void)action;
        (void)now;
        return HU_OK;
    }

    return HU_OK;
#endif
}

hu_error_t hu_research_dedup_finding(hu_allocator_t *alloc, sqlite3 *db,
                                      const char *source, size_t source_len,
                                      const char *finding, size_t finding_len,
                                      bool *is_duplicate) {
    (void)alloc;
    if (!db || !finding || finding_len == 0 || !is_duplicate)
        return HU_ERR_INVALID_ARGUMENT;

    *is_duplicate = false;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT 1 FROM research_findings WHERE "
        "((?1 IS NULL AND source IS NULL) OR (source = ?1)) AND finding = ?2 LIMIT 1",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_BACKEND;

    if (source && source_len > 0)
        sqlite3_bind_text(stmt, 1, source, (int)source_len, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 1);
    sqlite3_bind_text(stmt, 2, finding, (int)finding_len, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
        *is_duplicate = true;
    sqlite3_finalize(stmt);
    return HU_OK;
}

#endif /* HU_ENABLE_SQLITE */
