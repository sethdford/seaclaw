/*
 * Experience Distiller — mines recurring patterns from experience_log
 * and creates general lessons in the general_lessons table.
 */

#ifdef HU_ENABLE_SQLITE

#include "human/intelligence/distiller.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <ctype.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#define MAX_WORDS 256
#define WORD_MAX_LEN 64
#define LESSON_MAX_LEN 512

typedef struct {
    char word[WORD_MAX_LEN];
    size_t count;
} word_freq_t;

hu_error_t hu_distiller_init_tables(sqlite3 *db) {
    if (!db) return HU_ERR_INVALID_ARGUMENT;

    const char *sql_log =
        "CREATE TABLE IF NOT EXISTS experience_log("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "task TEXT NOT NULL, actions TEXT, outcome TEXT, "
        "score REAL, recorded_at INTEGER)";
    if (sqlite3_exec(db, sql_log, NULL, NULL, NULL) != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;

    const char *sql_lessons =
        "CREATE TABLE IF NOT EXISTS general_lessons("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "lesson TEXT UNIQUE NOT NULL, "
        "source TEXT DEFAULT 'distiller', "
        "occurrences INTEGER DEFAULT 1, "
        "created_at INTEGER)";
    if (sqlite3_exec(db, sql_lessons, NULL, NULL, NULL) != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;

    return HU_OK;
}

static int is_stop_word(const char *w, size_t len) {
    static const char *stops[] = {
        "the", "a", "an", "is", "was", "are", "were", "be", "been",
        "to", "of", "in", "for", "on", "with", "at", "by", "from",
        "it", "this", "that", "and", "or", "but", "not", "has", "had",
        "do", "did", "will", "would", "can", "could", "should", "may",
        "i", "we", "you", "he", "she", "they", "my", "our", "your",
        NULL
    };
    for (int i = 0; stops[i]; i++) {
        if (strlen(stops[i]) == len && memcmp(w, stops[i], len) == 0)
            return 1;
    }
    return 0;
}

static size_t extract_word_frequencies(const char *text, size_t text_len,
                                       word_freq_t *freqs, size_t max_freqs) {
    size_t unique = 0;
    const char *p = text;
    const char *end = text + text_len;

    while (p < end && unique < max_freqs) {
        while (p < end && !isalnum((unsigned char)*p)) p++;
        if (p >= end) break;
        const char *start = p;
        while (p < end && (isalnum((unsigned char)*p) || *p == '_' || *p == '-')) p++;
        size_t wlen = (size_t)(p - start);
        if (wlen < 3 || wlen >= WORD_MAX_LEN) continue;

        char lower[WORD_MAX_LEN];
        for (size_t j = 0; j < wlen; j++)
            lower[j] = (char)tolower((unsigned char)start[j]);
        lower[wlen] = '\0';

        if (is_stop_word(lower, wlen)) continue;

        int found = 0;
        for (size_t j = 0; j < unique; j++) {
            if (strlen(freqs[j].word) == wlen && memcmp(freqs[j].word, lower, wlen) == 0) {
                freqs[j].count++;
                found = 1;
                break;
            }
        }
        if (!found) {
            memcpy(freqs[unique].word, lower, wlen);
            freqs[unique].word[wlen] = '\0';
            freqs[unique].count = 1;
            unique++;
        }
    }
    return unique;
}

hu_error_t hu_experience_distill(hu_allocator_t *alloc, sqlite3 *db,
                                  size_t min_occurrences, int64_t now_ts,
                                  size_t *lessons_created) {
    if (!alloc || !db || !lessons_created)
        return HU_ERR_INVALID_ARGUMENT;
    *lessons_created = 0;

    if (min_occurrences == 0) min_occurrences = 2;

    /* Aggregate all experience text */
    const char *sel = "SELECT task, actions, outcome FROM experience_log ORDER BY recorded_at DESC LIMIT 500";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sel, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;

    word_freq_t *freqs = (word_freq_t *)alloc->alloc(alloc->ctx, MAX_WORDS * sizeof(word_freq_t));
    if (!freqs) {
        sqlite3_finalize(stmt);
        return HU_ERR_OUT_OF_MEMORY;
    }
    memset(freqs, 0, MAX_WORDS * sizeof(word_freq_t));
    size_t unique = 0;
    size_t row_count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        row_count++;
        for (int col = 0; col < 3; col++) {
            const char *text = (const char *)sqlite3_column_text(stmt, col);
            int bytes = sqlite3_column_bytes(stmt, col);
            if (text && bytes > 0) {
                word_freq_t local[MAX_WORDS];
                memset(local, 0, sizeof(local));
                size_t local_count = extract_word_frequencies(text, (size_t)bytes, local, MAX_WORDS);
                for (size_t j = 0; j < local_count && unique < MAX_WORDS; j++) {
                    int found = 0;
                    for (size_t k = 0; k < unique; k++) {
                        if (strcmp(freqs[k].word, local[j].word) == 0) {
                            freqs[k].count += local[j].count;
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        freqs[unique] = local[j];
                        unique++;
                    }
                }
            }
        }
    }
    sqlite3_finalize(stmt);

    if (row_count == 0) {
        alloc->free(alloc->ctx, freqs, MAX_WORDS * sizeof(word_freq_t));
        return HU_OK;
    }

    /* Insert lessons for words that appear >= min_occurrences */
    const char *ins =
        "INSERT OR IGNORE INTO general_lessons (lesson, source, occurrences, created_at) "
        "VALUES (?1, 'distiller', ?2, ?3)";
    sqlite3_stmt *ins_stmt = NULL;
    rc = sqlite3_prepare_v2(db, ins, -1, &ins_stmt, NULL);
    if (rc != SQLITE_OK) {
        alloc->free(alloc->ctx, freqs, MAX_WORDS * sizeof(word_freq_t));
        return HU_ERR_MEMORY_STORE;
    }

    for (size_t i = 0; i < unique; i++) {
        if (freqs[i].count >= min_occurrences) {
            char lesson[LESSON_MAX_LEN];
            int n = snprintf(lesson, sizeof(lesson),
                            "Experience shows: '%s' is a recurring pattern across %zu interactions.",
                            freqs[i].word, freqs[i].count);
            if (n <= 0) continue;

            sqlite3_bind_text(ins_stmt, 1, lesson, n, SQLITE_STATIC);
            sqlite3_bind_int(ins_stmt, 2, (int)freqs[i].count);
            sqlite3_bind_int64(ins_stmt, 3, now_ts);
            if (sqlite3_step(ins_stmt) == SQLITE_DONE) {
                if (sqlite3_changes(db) > 0)
                    (*lessons_created)++;
            }
            sqlite3_reset(ins_stmt);
            sqlite3_clear_bindings(ins_stmt);
        }
    }

    sqlite3_finalize(ins_stmt);
    alloc->free(alloc->ctx, freqs, MAX_WORDS * sizeof(word_freq_t));
    return HU_OK;
}

#endif /* HU_ENABLE_SQLITE */
