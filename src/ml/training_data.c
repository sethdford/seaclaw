/* Agent training data collection — trajectories and steps in SQLite.
 * When HU_ENABLE_SQLITE is not defined, this file is not compiled.
 */

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/ml/training_data.h"
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef HU_ENABLE_SQLITE

static const char *create_trajectories_sql =
    "CREATE TABLE IF NOT EXISTS trajectories ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "total_reward REAL,"
    "complete INTEGER,"
    "created_at INTEGER)";

static const char *create_steps_sql =
    "CREATE TABLE IF NOT EXISTS trajectory_steps ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "trajectory_id INTEGER,"
    "state TEXT,"
    "action TEXT,"
    "reward REAL,"
    "reward_type INTEGER,"
    "timestamp INTEGER)";

hu_error_t hu_training_data_init_tables(sqlite3 *db)
{
    if (!db)
        return HU_ERR_INVALID_ARGUMENT;
    int rc = sqlite3_exec(db, create_trajectories_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;
    rc = sqlite3_exec(db, create_steps_sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;
    return HU_OK;
}

hu_error_t hu_training_data_start_trajectory(hu_allocator_t *alloc, sqlite3 *db, int64_t *out_id)
{
    if (!alloc || !db || !out_id)
        return HU_ERR_INVALID_ARGUMENT;
    *out_id = 0;

    const char *sql = "INSERT INTO trajectories(total_reward,complete,created_at) VALUES(0,0,?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    int64_t now = (int64_t)time(NULL);
    sqlite3_bind_int64(stmt, 1, now);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return HU_ERR_IO;
    }
    *out_id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return HU_OK;
}

hu_error_t hu_training_data_record_step(hu_allocator_t *alloc, sqlite3 *db,
                                         int64_t trajectory_id,
                                         const char *state, size_t state_len,
                                         const char *action, size_t action_len,
                                         double reward, hu_reward_type_t type)
{
    (void)alloc;
    if (!db || trajectory_id <= 0)
        return HU_ERR_INVALID_ARGUMENT;

    const char *sql =
        "INSERT INTO trajectory_steps(trajectory_id,state,action,reward,reward_type,timestamp) "
        "VALUES(?,?,?,?,?,?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    int64_t now = (int64_t)time(NULL);
    sqlite3_bind_int64(stmt, 1, trajectory_id);
    sqlite3_bind_text(stmt, 2, state ? state : "", (int)(state ? state_len : 0), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, action ? action : "", (int)(action ? action_len : 0), SQLITE_STATIC);
    sqlite3_bind_double(stmt, 4, reward);
    sqlite3_bind_int(stmt, 5, (int)type);
    sqlite3_bind_int64(stmt, 6, now);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_training_data_end_trajectory(sqlite3 *db, int64_t trajectory_id, double total_reward)
{
    if (!db || trajectory_id <= 0)
        return HU_ERR_INVALID_ARGUMENT;

    const char *sql = "UPDATE trajectories SET total_reward=?, complete=1 WHERE id=?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    sqlite3_bind_double(stmt, 1, total_reward);
    sqlite3_bind_int64(stmt, 2, trajectory_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_training_data_export_json(hu_allocator_t *alloc, sqlite3 *db,
                                        char **out_json, size_t *out_len)
{
    if (!alloc || !db || !out_json || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out_json = NULL;
    *out_len = 0;

    const char *sql =
        "SELECT t.id, t.total_reward FROM trajectories t WHERE t.complete=1 ORDER BY t.id";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    size_t cap = 256;
    char *buf = (char *)alloc->alloc(alloc->ctx, cap);
    if (!buf) {
        sqlite3_finalize(stmt);
        return HU_ERR_OUT_OF_MEMORY;
    }
    size_t len = 0;
    buf[len++] = '[';

    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t tid = sqlite3_column_int64(stmt, 0);
        double total = sqlite3_column_double(stmt, 1);

        const char *steps_sql =
            "SELECT state,action,reward,reward_type,timestamp FROM trajectory_steps "
            "WHERE trajectory_id=? ORDER BY id";
        sqlite3_stmt *sstmt = NULL;
        if (sqlite3_prepare_v2(db, steps_sql, -1, &sstmt, NULL) != SQLITE_OK)
            continue;

        sqlite3_bind_int64(sstmt, 1, tid);

        if (!first)
            buf[len++] = ',';
        first = 0;

        size_t need = 64;
        if (len + need > cap) {
            size_t new_cap = cap * 2;
            char *nb = (char *)alloc->realloc(alloc->ctx, buf, cap, new_cap);
            if (!nb) {
                sqlite3_finalize(sstmt);
                sqlite3_finalize(stmt);
                alloc->free(alloc->ctx, buf, cap);
                return HU_ERR_OUT_OF_MEMORY;
            }
            buf = nb;
            cap = new_cap;
        }
        len = hu_buf_appendf(buf, cap, len,
                             "{\"id\":%lld,\"total_reward\":%.4f,\"steps\":[",
                             (long long)tid, total);

        int sfirst = 1;
        while (sqlite3_step(sstmt) == SQLITE_ROW) {
            const char *st = (const char *)sqlite3_column_text(sstmt, 0);
            const char *ac = (const char *)sqlite3_column_text(sstmt, 1);
            double r = sqlite3_column_double(sstmt, 2);
            int rt = sqlite3_column_int(sstmt, 3);
            int64_t ts = sqlite3_column_int64(sstmt, 4);

            if (!sfirst)
                buf[len++] = ',';
            sfirst = 0;

            hu_json_buf_t step = {0};
            hu_error_t jerr = hu_json_buf_init(&step, alloc);
            if (jerr == HU_OK)
                jerr = hu_json_buf_append_raw(&step, "{", 1);
            if (jerr == HU_OK)
                jerr = hu_json_append_key_value(&step, "state", 5, st ? st : "",
                                                st ? (size_t)sqlite3_column_bytes(sstmt, 0) : 0);
            if (jerr == HU_OK)
                jerr = hu_json_buf_append_raw(&step, ",", 1);
            if (jerr == HU_OK)
                jerr = hu_json_append_key_value(&step, "action", 6, ac ? ac : "",
                                                ac ? (size_t)sqlite3_column_bytes(sstmt, 1) : 0);
            if (jerr == HU_OK) {
                char tail[160];
                int tn = snprintf(tail, sizeof(tail),
                                  ",\"reward\":%.4f,\"reward_type\":%d,\"timestamp\":%lld}", r, rt,
                                  (long long)ts);
                if (tn > 0 && (size_t)tn < sizeof(tail))
                    jerr = hu_json_buf_append_raw(&step, tail, (size_t)tn);
                else
                    jerr = HU_ERR_INTERNAL;
            }
            if (jerr != HU_OK) {
                hu_json_buf_free(&step);
                sqlite3_finalize(sstmt);
                sqlite3_finalize(stmt);
                alloc->free(alloc->ctx, buf, cap);
                return jerr;
            }
            need = step.len + 1;
            while (len + need > cap) {
                if (cap > SIZE_MAX / 2) {
                    hu_json_buf_free(&step);
                    sqlite3_finalize(sstmt);
                    sqlite3_finalize(stmt);
                    alloc->free(alloc->ctx, buf, cap);
                    return HU_ERR_OUT_OF_MEMORY;
                }
                size_t new_cap = cap * 2;
                char *nb = (char *)alloc->realloc(alloc->ctx, buf, cap, new_cap);
                if (!nb) {
                    hu_json_buf_free(&step);
                    sqlite3_finalize(sstmt);
                    sqlite3_finalize(stmt);
                    alloc->free(alloc->ctx, buf, cap);
                    return HU_ERR_OUT_OF_MEMORY;
                }
                buf = nb;
                cap = new_cap;
            }
            memcpy(buf + len, step.ptr, step.len);
            len += step.len;
            hu_json_buf_free(&step);
        }
        sqlite3_finalize(sstmt);
        buf[len++] = ']';
        buf[len++] = '}';
    }
    sqlite3_finalize(stmt);
    buf[len++] = ']';
    buf[len] = '\0';

    *out_json = buf;
    *out_len = len;
    return HU_OK;
}

hu_error_t hu_training_data_count(sqlite3 *db, size_t *out_count)
{
    if (!db || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_count = 0;

    const char *sql = "SELECT COUNT(*) FROM trajectories WHERE complete=1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
        *out_count = (size_t)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return HU_OK;
}

static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_alnum_or_dot(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}

hu_error_t hu_training_data_strip_pii(char *text, size_t text_len, size_t *out_len)
{
    if (!text || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    size_t wi = 0;
    size_t ri = 0;
    const char *email_rep = "[EMAIL]";
    const char *phone_rep = "[PHONE]";
    const char *card_rep = "[CARD]";
    const size_t email_len = 7;
    const size_t phone_len = 8;
    const size_t card_len = 6;

    while (ri < text_len) {
        int replaced = 0;

        /* Email: x@y.z */
        if (ri + 5 < text_len && text[ri] != '@') {
            size_t at = ri;
            while (at < text_len && text[at] != '@') at++;
            if (at < text_len && at > ri) {
                size_t dot = at + 1;
                while (dot < text_len && text[dot] != '.') dot++;
                if (dot < text_len && dot > at + 1 && dot + 1 < text_len) {
                    size_t j;
                    for (j = ri; j <= dot + 1 && j < text_len; j++) {
                        if (!is_alnum_or_dot(text[j]) && text[j] != '@')
                            break;
                    }
                    if (j > dot + 1) {
                        for (size_t k = 0; k < email_len && wi + k < text_len; k++)
                            text[wi + k] = email_rep[k];
                        wi += email_len;
                        ri = dot + 1;
                        while (ri < text_len && (is_alnum_or_dot(text[ri]) || text[ri] == '@'))
                            ri++;
                        replaced = 1;
                    }
                }
            }
        }

        /* Phone: xxx-xxx-xxxx */
        if (!replaced && ri + 12 <= text_len && is_digit(text[ri]) && is_digit(text[ri + 1]) &&
            is_digit(text[ri + 2]) && text[ri + 3] == '-' && is_digit(text[ri + 4]) &&
            is_digit(text[ri + 5]) && is_digit(text[ri + 6]) && text[ri + 7] == '-' &&
            is_digit(text[ri + 8]) && is_digit(text[ri + 9]) && is_digit(text[ri + 10]) &&
            is_digit(text[ri + 11])) {
            for (size_t k = 0; k < phone_len && wi + k < text_len; k++)
                text[wi + k] = phone_rep[k];
            wi += phone_len;
            ri += 12;
            replaced = 1;
        }

        /* Phone: (xxx) xxx-xxxx */
        if (!replaced && ri + 14 <= text_len && text[ri] == '(' && is_digit(text[ri + 1]) &&
            is_digit(text[ri + 2]) && is_digit(text[ri + 3]) && text[ri + 4] == ')' &&
            text[ri + 5] == ' ' && is_digit(text[ri + 6]) && is_digit(text[ri + 7]) &&
            is_digit(text[ri + 8]) && text[ri + 9] == '-' && is_digit(text[ri + 10]) &&
            is_digit(text[ri + 11]) && is_digit(text[ri + 12]) && is_digit(text[ri + 13])) {
            for (size_t k = 0; k < phone_len && wi + k < text_len; k++)
                text[wi + k] = phone_rep[k];
            wi += phone_len;
            ri += 14;
            replaced = 1;
        }

        /* Card: 16 consecutive digits (possibly with spaces/dashes) */
        if (!replaced && ri + 16 <= text_len) {
            size_t dcount = 0;
            size_t scan = ri;
            while (scan < text_len && (is_digit(text[scan]) || text[scan] == ' ' || text[scan] == '-')) {
                if (is_digit(text[scan]))
                    dcount++;
                scan++;
                if (dcount == 16)
                    break;
            }
            if (dcount == 16) {
                for (size_t k = 0; k < card_len && wi + k < text_len; k++)
                    text[wi + k] = card_rep[k];
                wi += card_len;
                ri = scan;
                replaced = 1;
            }
        }

        if (!replaced) {
            text[wi++] = text[ri++];
        }
    }
    text[wi] = '\0';
    *out_len = wi;
    return HU_OK;
}

#endif /* HU_ENABLE_SQLITE */
