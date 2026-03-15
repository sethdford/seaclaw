/*
 * Online Learning Engine — extracts learning signals from every interaction
 * and updates strategy weights in real-time via EMA.
 */

#ifdef HU_ENABLE_SQLITE

#include "human/intelligence/online_learning.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/string.h"
#include <sqlite3.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define STRATEGY_MAX 128
#define CONTEXT_MAX 512

hu_error_t hu_online_learning_create(hu_allocator_t *alloc, sqlite3 *db,
                                     double learning_rate,
                                     hu_online_learning_t *out) {
    if (!alloc || !db || !out)
        return HU_ERR_INVALID_ARGUMENT;
    out->alloc = alloc;
    out->db = db;
    out->learning_rate = (learning_rate <= 0.0) ? 0.1 : learning_rate;
    return HU_OK;
}

void hu_online_learning_deinit(hu_online_learning_t *engine) {
    (void)engine;
}

hu_error_t hu_online_learning_init_tables(hu_online_learning_t *engine) {
    if (!engine || !engine->db)
        return HU_ERR_INVALID_ARGUMENT;
    const char *sql_signals =
        "CREATE TABLE IF NOT EXISTS learning_signals("
        "id INTEGER PRIMARY KEY, type INTEGER, context TEXT, tool_name TEXT, "
        "magnitude REAL, timestamp INTEGER)";
    int rc = sqlite3_exec(engine->db, sql_signals, NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    const char *sql_weights =
        "CREATE TABLE IF NOT EXISTS strategy_weights("
        "strategy TEXT PRIMARY KEY, weight REAL DEFAULT 1.0, updated_at INTEGER)";
    rc = sqlite3_exec(engine->db, sql_weights, NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    return HU_OK;
}

hu_error_t hu_online_learning_record(hu_online_learning_t *engine,
                                     const hu_learning_signal_t *signal) {
    if (!engine || !engine->db || !signal)
        return HU_ERR_INVALID_ARGUMENT;
    const char *sql =
        "INSERT INTO learning_signals (type, context, tool_name, magnitude, timestamp) "
        "VALUES (?1, ?2, ?3, ?4, ?5)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(engine->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    char ctx_buf[CONTEXT_MAX + 1];
    char tool_buf[128];
    sqlite3_bind_int(stmt, 1, (int)signal->type);
    if (signal->context_len > 0) {
        size_t copy_len = signal->context_len < CONTEXT_MAX ? signal->context_len : CONTEXT_MAX;
        memcpy(ctx_buf, signal->context, copy_len);
        ctx_buf[copy_len] = '\0';
        sqlite3_bind_text(stmt, 2, ctx_buf, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    if (signal->tool_name_len > 0) {
        size_t copy_len = signal->tool_name_len < 127 ? signal->tool_name_len : 127;
        memcpy(tool_buf, signal->tool_name, copy_len);
        tool_buf[copy_len] = '\0';
        sqlite3_bind_text(stmt, 3, tool_buf, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    sqlite3_bind_double(stmt, 4, signal->magnitude);
    sqlite3_bind_int64(stmt, 5, signal->timestamp);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return HU_ERR_MEMORY_STORE;

    /* Auto-update strategy weight from signal (closes loop: signals -> weights) */
    char strategy_buf[STRATEGY_MAX];
    const char *strategy = NULL;
    size_t strategy_len = 0;
    double evidence = signal->magnitude;

    switch (signal->type) {
    case HU_SIGNAL_TOOL_SUCCESS:
        if (signal->tool_name_len > 0) {
            size_t tn = signal->tool_name_len < (STRATEGY_MAX - 6u)
                            ? signal->tool_name_len
                            : (size_t)(STRATEGY_MAX - 6);
            memcpy(strategy_buf, "tool:", 5);
            memcpy(strategy_buf + 5, signal->tool_name, tn);
            strategy_buf[5 + tn] = '\0';
            strategy = strategy_buf;
            strategy_len = 5 + tn;
            evidence = signal->magnitude;
        }
        break;
    case HU_SIGNAL_TOOL_FAILURE:
        if (signal->tool_name_len > 0) {
            size_t tn = signal->tool_name_len < (STRATEGY_MAX - 6u)
                            ? signal->tool_name_len
                            : (size_t)(STRATEGY_MAX - 6);
            memcpy(strategy_buf, "tool:", 5);
            memcpy(strategy_buf + 5, signal->tool_name, tn);
            strategy_buf[5 + tn] = '\0';
            strategy = strategy_buf;
            strategy_len = 5 + tn;
            evidence = 1.0 - signal->magnitude;
        }
        break;
    case HU_SIGNAL_USER_APPROVAL:
        strategy = "user_interaction";
        strategy_len = 16;
        evidence = signal->magnitude;
        break;
    case HU_SIGNAL_USER_CORRECTION:
        strategy = "user_interaction";
        strategy_len = 16;
        evidence = 1.0 - signal->magnitude;
        break;
    case HU_SIGNAL_LONG_RESPONSE:
        strategy = "response_length";
        strategy_len = 15;
        evidence = signal->magnitude;
        break;
    case HU_SIGNAL_SHORT_RESPONSE:
        strategy = "response_length";
        strategy_len = 15;
        evidence = 1.0 - signal->magnitude;
        break;
    default:
        break;
    }

    if (strategy && strategy_len > 0) {
        (void)hu_online_learning_update_weight(engine, strategy, strategy_len,
                                               evidence, signal->timestamp);
    }

    return HU_OK;
}

hu_error_t hu_online_learning_update_weight(hu_online_learning_t *engine,
                                            const char *strategy, size_t len,
                                            double new_evidence, int64_t now_ts) {
    if (!engine || !engine->db || !strategy)
        return HU_ERR_INVALID_ARGUMENT;
    if (len == 0)
        len = strlen(strategy);
    if (len > STRATEGY_MAX - 1)
        len = STRATEGY_MAX - 1;
    char strat_buf[STRATEGY_MAX];
    memcpy(strat_buf, strategy, len);
    strat_buf[len] = '\0';
    const char *sel = "SELECT weight FROM strategy_weights WHERE strategy = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(engine->db, sel, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    sqlite3_bind_text(stmt, 1, strat_buf, -1, SQLITE_STATIC);
    double old_weight = 1.0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        old_weight = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);
    double lr = engine->learning_rate;
    double new_weight = (1.0 - lr) * old_weight + lr * new_evidence;
    const char *upsert =
        "INSERT OR REPLACE INTO strategy_weights (strategy, weight, updated_at) "
        "VALUES (?1, ?2, ?3)";
    rc = sqlite3_prepare_v2(engine->db, upsert, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    sqlite3_bind_text(stmt, 1, strat_buf, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, new_weight);
    sqlite3_bind_int64(stmt, 3, now_ts);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_MEMORY_STORE;
}

double hu_online_learning_get_weight(hu_online_learning_t *engine,
                                     const char *strategy, size_t len) {
    if (!engine || !engine->db || !strategy)
        return 1.0;
    if (len == 0)
        len = strlen(strategy);
    if (len > STRATEGY_MAX - 1)
        len = STRATEGY_MAX - 1;
    char strat_buf[STRATEGY_MAX];
    memcpy(strat_buf, strategy, len);
    strat_buf[len] = '\0';
    const char *sql = "SELECT weight FROM strategy_weights WHERE strategy = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(engine->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return 1.0;
    sqlite3_bind_text(stmt, 1, strat_buf, -1, SQLITE_STATIC);
    double weight = 1.0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        weight = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);
    return weight;
}

hu_error_t hu_online_learning_signal_count(hu_online_learning_t *engine,
                                           size_t *out) {
    if (!engine || !engine->db || !out)
        return HU_ERR_INVALID_ARGUMENT;
    *out = 0;
    const char *sql = "SELECT COUNT(*) FROM learning_signals";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(engine->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        *out = (size_t)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return HU_OK;
}

hu_error_t hu_online_learning_build_context(hu_online_learning_t *engine,
                                            char **out, size_t *out_len) {
    if (!engine || !engine->db || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;
    const char *sql =
        "SELECT strategy, weight FROM strategy_weights "
        "ORDER BY ABS(weight - 1.0) DESC LIMIT 5";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(engine->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    size_t count = 0;
    char *lines[5];
    size_t line_lens[5];
    for (size_t i = 0; i < 5; i++)
        lines[i] = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < 5) {
        const char *strat = (const char *)sqlite3_column_text(stmt, 0);
        double weight = sqlite3_column_double(stmt, 1);
        if (!strat)
            continue;
        int n = snprintf(NULL, 0, "- %s: %.2f\n", strat, weight);
        if (n < 0)
            break;
        size_t need = (size_t)n + 1;
        lines[count] = (char *)engine->alloc->alloc(engine->alloc->ctx, need);
        if (!lines[count])
            break;
        snprintf(lines[count], need, "- %s: %.2f\n", strat, weight);
        line_lens[count] = (size_t)n;
        count++;
    }
    sqlite3_finalize(stmt);
    if (count == 0) {
        char *empty = (char *)engine->alloc->alloc(engine->alloc->ctx, 1);
        if (!empty)
            return HU_ERR_OUT_OF_MEMORY;
        empty[0] = '\0';
        *out = empty;
        *out_len = 0;
        return HU_OK;
    }
    size_t total = 20;
    for (size_t i = 0; i < count; i++)
        total += line_lens[i];
    char *buf = (char *)engine->alloc->alloc(engine->alloc->ctx, total + 1);
    if (!buf) {
        for (size_t i = 0; i < count; i++)
            engine->alloc->free(engine->alloc->ctx, lines[i], line_lens[i] + 1);
        return HU_ERR_OUT_OF_MEMORY;
    }
    size_t pos = 0;
    memcpy(buf, "Learned preferences:\n", 20);
    pos += 20;
    for (size_t i = 0; i < count; i++) {
        memcpy(buf + pos, lines[i], line_lens[i]);
        pos += line_lens[i];
        engine->alloc->free(engine->alloc->ctx, lines[i], line_lens[i] + 1);
    }
    buf[pos] = '\0';
    *out = buf;
    *out_len = pos;
    return HU_OK;
}

double hu_online_learning_response_quality(const hu_learning_signal_t *signals,
                                            size_t count) {
    if (!signals || count == 0)
        return 0.5;
    double score = 0.5;
    for (size_t i = 0; i < count; i++) {
        switch (signals[i].type) {
        case HU_SIGNAL_TOOL_SUCCESS:
        case HU_SIGNAL_USER_APPROVAL:
            score += 0.2;
            break;
        case HU_SIGNAL_TOOL_FAILURE:
        case HU_SIGNAL_USER_CORRECTION:
            score -= 0.2;
            break;
        case HU_SIGNAL_LONG_RESPONSE:
        case HU_SIGNAL_SHORT_RESPONSE:
            break;
        }
    }
    if (score < 0.0)
        score = 0.0;
    if (score > 1.0)
        score = 1.0;
    return score;
}

const char *hu_signal_type_str(hu_signal_type_t type) {
    switch (type) {
    case HU_SIGNAL_TOOL_SUCCESS:
        return "tool_success";
    case HU_SIGNAL_TOOL_FAILURE:
        return "tool_failure";
    case HU_SIGNAL_USER_CORRECTION:
        return "user_correction";
    case HU_SIGNAL_USER_APPROVAL:
        return "user_approval";
    case HU_SIGNAL_LONG_RESPONSE:
        return "long_response";
    case HU_SIGNAL_SHORT_RESPONSE:
        return "short_response";
    default:
        return "unknown";
    }
}

#endif /* HU_ENABLE_SQLITE */
