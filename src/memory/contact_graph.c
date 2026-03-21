#include "human/memory/contact_graph.h"
#include "human/core/error.h"
#include <string.h>

size_t hu_contact_normalize_phone(const char *input, char *out_buf, size_t out_cap) {
    if (!input || !out_buf || out_cap == 0)
        return 0;
    size_t pos = 0;
    for (size_t i = 0; input[i] && pos < out_cap - 1; i++) {
        if (input[i] >= '0' && input[i] <= '9') {
            out_buf[pos++] = input[i];
        } else if (input[i] == '+' && pos == 0) {
            out_buf[pos++] = '+';
        }
    }
    out_buf[pos] = '\0';
    return pos;
}

#ifdef HU_ENABLE_SQLITE

#include <sqlite3.h>
#include <stdio.h>

#define HU_CONTACT_GRAPH_COPY_COL(dst, dst_size, text, nbytes) \
    do { \
        size_t _nb = (nbytes); \
        size_t _copy = _nb < (dst_size) - 1 ? _nb : (dst_size) - 1; \
        if ((text) && _copy > 0) { \
            memcpy((dst), (text), _copy); \
            (dst)[_copy] = '\0'; \
        } else { \
            (dst)[0] = '\0'; \
        } \
    } while (0)

static int non_empty_cstr(const char *s) {
    return s && s[0] != '\0';
}

hu_error_t hu_contact_graph_init(hu_allocator_t *alloc, void *db) {
    if (!alloc)
        return HU_ERR_INVALID_ARGUMENT;
    if (!db)
        return HU_ERR_INVALID_ARGUMENT;

    sqlite3 *sdb = (sqlite3 *)db;
    static const char k_schema[] =
        "CREATE TABLE IF NOT EXISTS contact_identities ("
        "contact_id TEXT,"
        "display_name TEXT,"
        "platform TEXT NOT NULL,"
        "platform_handle TEXT NOT NULL,"
        "confidence REAL DEFAULT 1.0,"
        "PRIMARY KEY(platform, platform_handle)"
        ")";
    char *err = NULL;
    int rc = sqlite3_exec(sdb, k_schema, NULL, NULL, &err);
    if (err) {
        sqlite3_free(err);
    }
    return (rc == SQLITE_OK) ? HU_OK : HU_ERR_MEMORY_BACKEND;
}

hu_error_t hu_contact_graph_link(void *db, const char *contact_id, const char *platform,
    const char *platform_handle, const char *display_name, double confidence) {
    if (!db || !non_empty_cstr(contact_id) || !non_empty_cstr(platform) ||
        !non_empty_cstr(platform_handle))
        return HU_ERR_INVALID_ARGUMENT;

    sqlite3 *sdb = (sqlite3 *)db;
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(sdb,
        "INSERT OR REPLACE INTO contact_identities(contact_id,display_name,platform,"
        "platform_handle,confidence) VALUES(?,?,?,?,?)",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_BACKEND;

    sqlite3_bind_text(stmt, 1, contact_id, -1, NULL);
    if (display_name && display_name[0] != '\0')
        sqlite3_bind_text(stmt, 2, display_name, -1, NULL);
    else
        sqlite3_bind_text(stmt, 2, "", 0, NULL);
    sqlite3_bind_text(stmt, 3, platform, -1, NULL);
    sqlite3_bind_text(stmt, 4, platform_handle, -1, NULL);
    sqlite3_bind_double(stmt, 5, confidence);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_MEMORY_BACKEND;
}

hu_error_t hu_contact_graph_resolve(void *db, const char *platform, const char *platform_handle,
    char *out_contact_id, size_t out_size) {
    if (!db || !non_empty_cstr(platform) || !non_empty_cstr(platform_handle) || !out_contact_id ||
        out_size == 0)
        return HU_ERR_INVALID_ARGUMENT;

    sqlite3 *sdb = (sqlite3 *)db;
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(sdb,
        "SELECT contact_id FROM contact_identities WHERE platform=? AND platform_handle=?", -1,
        &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_BACKEND;

    sqlite3_bind_text(stmt, 1, platform, -1, NULL);
    sqlite3_bind_text(stmt, 2, platform_handle, -1, NULL);

    hu_error_t err = HU_ERR_NOT_FOUND;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *cid = (const char *)sqlite3_column_text(stmt, 0);
        size_t nbytes = cid ? (size_t)sqlite3_column_bytes(stmt, 0) : 0;
        HU_CONTACT_GRAPH_COPY_COL(out_contact_id, out_size, cid, nbytes);
        err = HU_OK;
    }
    sqlite3_finalize(stmt);
    return err;
}

hu_error_t hu_contact_graph_list(hu_allocator_t *alloc, void *db, const char *contact_id,
    hu_contact_identity_t **out, size_t *out_count) {
    if (!alloc || !db || !non_empty_cstr(contact_id) || !out || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0;

    sqlite3 *sdb = (sqlite3 *)db;
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(sdb,
        "SELECT contact_id,display_name,platform,platform_handle,confidence "
        "FROM contact_identities WHERE contact_id=?",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_BACKEND;

    sqlite3_bind_text(stmt, 1, contact_id, -1, NULL);

    size_t cap = 8;
    size_t count = 0;
    hu_contact_identity_t *arr =
        (hu_contact_identity_t *)alloc->alloc(alloc->ctx, cap * sizeof(hu_contact_identity_t));
    if (!arr) {
        sqlite3_finalize(stmt);
        return HU_ERR_OUT_OF_MEMORY;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            size_t new_cap = cap * 2;
            hu_contact_identity_t *n = (hu_contact_identity_t *)alloc->alloc(
                alloc->ctx, new_cap * sizeof(hu_contact_identity_t));
            if (!n) {
                alloc->free(alloc->ctx, arr, cap * sizeof(hu_contact_identity_t));
                sqlite3_finalize(stmt);
                return HU_ERR_OUT_OF_MEMORY;
            }
            memcpy(n, arr, count * sizeof(hu_contact_identity_t));
            alloc->free(alloc->ctx, arr, cap * sizeof(hu_contact_identity_t));
            arr = n;
            cap = new_cap;
        }
        hu_contact_identity_t *e = &arr[count];
        memset(e, 0, sizeof(*e));

        const char *c0 = (const char *)sqlite3_column_text(stmt, 0);
        size_t n0 = c0 ? (size_t)sqlite3_column_bytes(stmt, 0) : 0;
        HU_CONTACT_GRAPH_COPY_COL(e->contact_id, sizeof(e->contact_id), c0, n0);
        const char *c1 = (const char *)sqlite3_column_text(stmt, 1);
        size_t n1 = c1 ? (size_t)sqlite3_column_bytes(stmt, 1) : 0;
        HU_CONTACT_GRAPH_COPY_COL(e->display_name, sizeof(e->display_name), c1, n1);
        const char *c2 = (const char *)sqlite3_column_text(stmt, 2);
        size_t n2 = c2 ? (size_t)sqlite3_column_bytes(stmt, 2) : 0;
        HU_CONTACT_GRAPH_COPY_COL(e->platform, sizeof(e->platform), c2, n2);
        const char *c3 = (const char *)sqlite3_column_text(stmt, 3);
        size_t n3 = c3 ? (size_t)sqlite3_column_bytes(stmt, 3) : 0;
        HU_CONTACT_GRAPH_COPY_COL(e->platform_handle, sizeof(e->platform_handle), c3, n3);
        e->confidence = sqlite3_column_double(stmt, 4);
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0 && arr) {
        alloc->free(alloc->ctx, arr, cap * sizeof(hu_contact_identity_t));
        arr = NULL;
    }
    *out = arr;
    *out_count = count;
    return HU_OK;
}

hu_error_t hu_contact_graph_merge(void *db, const char *old_id, const char *new_id) {
    if (!db || !non_empty_cstr(old_id) || !non_empty_cstr(new_id))
        return HU_ERR_INVALID_ARGUMENT;

    sqlite3 *sdb = (sqlite3 *)db;
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(sdb,
        "UPDATE contact_identities SET contact_id=? WHERE contact_id=?", -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_BACKEND;

    sqlite3_bind_text(stmt, 1, new_id, -1, NULL);
    sqlite3_bind_text(stmt, 2, old_id, -1, NULL);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_MEMORY_BACKEND;
}

hu_error_t hu_contact_graph_auto_resolve(hu_allocator_t *alloc, void *db,
    const char *display_name, const char *platform_handle,
    hu_contact_identity_t **out_candidates, size_t *out_count) {
    if (!alloc || !out_candidates || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_candidates = NULL;
    *out_count = 0;

#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)db;
    (void)display_name;
    (void)platform_handle;
    return HU_OK;
#else
    if (!db)
        return HU_ERR_NOT_SUPPORTED;

    sqlite3 *sdb = (sqlite3 *)db;

    /* Collect candidates from multiple matching strategies */
    hu_contact_identity_t candidates[32];
    size_t count = 0;

    /* Strategy 1: exact platform_handle match on any platform */
    if (platform_handle && platform_handle[0]) {
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(sdb,
                "SELECT contact_id, display_name, platform, platform_handle, confidence "
                "FROM contact_identities WHERE platform_handle = ? LIMIT 10",
                -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, platform_handle, -1, SQLITE_STATIC);
            while (sqlite3_step(stmt) == SQLITE_ROW && count < 32) {
                hu_contact_identity_t *c = &candidates[count];
                const char *cid = (const char *)sqlite3_column_text(stmt, 0);
                const char *dn = (const char *)sqlite3_column_text(stmt, 1);
                const char *pl = (const char *)sqlite3_column_text(stmt, 2);
                const char *ph = (const char *)sqlite3_column_text(stmt, 3);
                c->confidence = sqlite3_column_double(stmt, 4);
                size_t n0 = cid ? (size_t)sqlite3_column_bytes(stmt, 0) : 0;
                HU_CONTACT_GRAPH_COPY_COL(c->contact_id, sizeof(c->contact_id), cid, n0);
                size_t n1 = dn ? (size_t)sqlite3_column_bytes(stmt, 1) : 0;
                HU_CONTACT_GRAPH_COPY_COL(c->display_name, sizeof(c->display_name), dn, n1);
                size_t n2 = pl ? (size_t)sqlite3_column_bytes(stmt, 2) : 0;
                HU_CONTACT_GRAPH_COPY_COL(c->platform, sizeof(c->platform), pl, n2);
                size_t n3 = ph ? (size_t)sqlite3_column_bytes(stmt, 3) : 0;
                HU_CONTACT_GRAPH_COPY_COL(c->platform_handle, sizeof(c->platform_handle), ph, n3);
                count++;
            }
            sqlite3_finalize(stmt);
        }
    }

    /* Strategy 2: normalized phone match */
    if (platform_handle && platform_handle[0] && count < 32) {
        char norm[32];
        size_t nlen = hu_contact_normalize_phone(platform_handle, norm, sizeof(norm));
        if (nlen >= 7) {
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(sdb,
                    "SELECT contact_id, display_name, platform, platform_handle, confidence "
                    "FROM contact_identities LIMIT 200",
                    -1, &stmt, NULL) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW && count < 32) {
                    const char *ph = (const char *)sqlite3_column_text(stmt, 3);
                    if (!ph)
                        continue;
                    char other_norm[32];
                    size_t olen = hu_contact_normalize_phone(ph, other_norm, sizeof(other_norm));
                    if (olen < 7 || olen != nlen || memcmp(norm, other_norm, nlen) != 0)
                        continue;
                    /* Check for duplicate */
                    const char *cid = (const char *)sqlite3_column_text(stmt, 0);
                    int dup = 0;
                    for (size_t d = 0; d < count; d++) {
                        if (cid && strcmp(candidates[d].contact_id, cid) == 0 &&
                            strcmp(candidates[d].platform_handle, ph) == 0) {
                            dup = 1;
                            break;
                        }
                    }
                    if (dup)
                        continue;
                    hu_contact_identity_t *c = &candidates[count];
                    const char *dn = (const char *)sqlite3_column_text(stmt, 1);
                    const char *pl = (const char *)sqlite3_column_text(stmt, 2);
                    c->confidence = sqlite3_column_double(stmt, 4) * 0.8;
                    size_t nc0 = cid ? (size_t)sqlite3_column_bytes(stmt, 0) : 0;
                    HU_CONTACT_GRAPH_COPY_COL(c->contact_id, sizeof(c->contact_id), cid, nc0);
                    size_t nc1 = dn ? (size_t)sqlite3_column_bytes(stmt, 1) : 0;
                    HU_CONTACT_GRAPH_COPY_COL(c->display_name, sizeof(c->display_name), dn, nc1);
                    size_t nc2 = pl ? (size_t)sqlite3_column_bytes(stmt, 2) : 0;
                    HU_CONTACT_GRAPH_COPY_COL(c->platform, sizeof(c->platform), pl, nc2);
                    size_t nc3 = ph ? (size_t)sqlite3_column_bytes(stmt, 3) : 0;
                    HU_CONTACT_GRAPH_COPY_COL(c->platform_handle, sizeof(c->platform_handle), ph, nc3);
                    count++;
                }
                sqlite3_finalize(stmt);
            }
        }
    }

    /* Strategy 3: display_name case-insensitive match */
    if (display_name && display_name[0] && count < 32) {
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(sdb,
                "SELECT contact_id, display_name, platform, platform_handle, confidence "
                "FROM contact_identities WHERE display_name LIKE ? COLLATE NOCASE LIMIT 10",
                -1, &stmt, NULL) == SQLITE_OK) {
            char pattern[260];
            int pn = snprintf(pattern, sizeof(pattern), "%%%s%%", display_name);
            if (pn > 0 && (size_t)pn < sizeof(pattern))
                sqlite3_bind_text(stmt, 1, pattern, pn, SQLITE_STATIC);
            else
                sqlite3_bind_text(stmt, 1, display_name, -1, SQLITE_STATIC);
            while (sqlite3_step(stmt) == SQLITE_ROW && count < 32) {
                const char *cid = (const char *)sqlite3_column_text(stmt, 0);
                const char *ph = (const char *)sqlite3_column_text(stmt, 3);
                int dup = 0;
                for (size_t d = 0; d < count; d++) {
                    if (cid && strcmp(candidates[d].contact_id, cid) == 0 && ph &&
                        strcmp(candidates[d].platform_handle, ph) == 0) {
                        dup = 1;
                        break;
                    }
                }
                if (dup)
                    continue;
                hu_contact_identity_t *c = &candidates[count];
                const char *dn = (const char *)sqlite3_column_text(stmt, 1);
                const char *pl = (const char *)sqlite3_column_text(stmt, 2);
                c->confidence = sqlite3_column_double(stmt, 4) * 0.5;
                size_t nc0 = cid ? (size_t)sqlite3_column_bytes(stmt, 0) : 0;
                HU_CONTACT_GRAPH_COPY_COL(c->contact_id, sizeof(c->contact_id), cid, nc0);
                size_t nc1 = dn ? (size_t)sqlite3_column_bytes(stmt, 1) : 0;
                HU_CONTACT_GRAPH_COPY_COL(c->display_name, sizeof(c->display_name), dn, nc1);
                size_t nc2 = pl ? (size_t)sqlite3_column_bytes(stmt, 2) : 0;
                HU_CONTACT_GRAPH_COPY_COL(c->platform, sizeof(c->platform), pl, nc2);
                size_t nc3 = ph ? (size_t)sqlite3_column_bytes(stmt, 3) : 0;
                HU_CONTACT_GRAPH_COPY_COL(c->platform_handle, sizeof(c->platform_handle), ph, nc3);
                count++;
            }
            sqlite3_finalize(stmt);
        }
    }

    if (count == 0)
        return HU_OK;

    /* Sort by confidence descending (simple insertion sort) */
    for (size_t i = 1; i < count; i++) {
        hu_contact_identity_t tmp = candidates[i];
        size_t j = i;
        while (j > 0 && candidates[j - 1].confidence < tmp.confidence) {
            candidates[j] = candidates[j - 1];
            j--;
        }
        candidates[j] = tmp;
    }

    hu_contact_identity_t *out = (hu_contact_identity_t *)alloc->alloc(
        alloc->ctx, count * sizeof(hu_contact_identity_t));
    if (!out)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(out, candidates, count * sizeof(hu_contact_identity_t));
    *out_candidates = out;
    *out_count = count;
    return HU_OK;
#endif
}

#elif defined(HU_IS_TEST)

enum { hu_contact_graph_test_cap = 64 };

typedef struct {
    bool used;
    hu_contact_identity_t row;
} hu_contact_graph_test_slot_t;

static hu_contact_graph_test_slot_t g_contact_graph_test_rows[hu_contact_graph_test_cap];

static int test_non_empty(const char *s) {
    return s && s[0] != '\0';
}

static void test_copy_str(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0)
        return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    size_t copy = len < dst_size - 1 ? len : dst_size - 1;
    if (copy > 0)
        memcpy(dst, src, copy);
    dst[copy] = '\0';
}

static int test_find_pk(const char *platform, const char *platform_handle) {
    for (int i = 0; i < hu_contact_graph_test_cap; i++) {
        if (!g_contact_graph_test_rows[i].used)
            continue;
        if (strcmp(g_contact_graph_test_rows[i].row.platform, platform) == 0 &&
            strcmp(g_contact_graph_test_rows[i].row.platform_handle, platform_handle) == 0)
            return i;
    }
    return -1;
}

static int test_find_empty(void) {
    for (int i = 0; i < hu_contact_graph_test_cap; i++) {
        if (!g_contact_graph_test_rows[i].used)
            return i;
    }
    return -1;
}

static unsigned char test_ascii_tolower(unsigned char c) {
    if (c >= 'A' && c <= 'Z')
        return (unsigned char)(c - 'A' + 'a');
    return c;
}

static int test_substring_ci(const char *haystack, const char *needle) {
    if (!haystack || !needle || !needle[0])
        return 0;
    for (size_t i = 0; haystack[i]; i++) {
        size_t j = 0;
        while (needle[j] && haystack[i + j]) {
            if (test_ascii_tolower((unsigned char)needle[j]) !=
                test_ascii_tolower((unsigned char)haystack[i + j]))
                break;
            j++;
        }
        if (needle[j] == '\0')
            return 1;
    }
    return 0;
}

static int test_candidate_dup(const hu_contact_identity_t *candidates, size_t count,
    const char *cid, const char *ph) {
    for (size_t d = 0; d < count; d++) {
        if (cid && strcmp(candidates[d].contact_id, cid) == 0 && ph &&
            strcmp(candidates[d].platform_handle, ph) == 0)
            return 1;
    }
    return 0;
}

hu_error_t hu_contact_graph_init(hu_allocator_t *alloc, void *db) {
    (void)db;
    if (!alloc)
        return HU_ERR_INVALID_ARGUMENT;
    memset(g_contact_graph_test_rows, 0, sizeof(g_contact_graph_test_rows));
    return HU_OK;
}

hu_error_t hu_contact_graph_link(void *db, const char *contact_id, const char *platform,
    const char *platform_handle, const char *display_name, double confidence) {
    (void)db;
    if (!test_non_empty(contact_id) || !test_non_empty(platform) || !test_non_empty(platform_handle))
        return HU_ERR_INVALID_ARGUMENT;

    int idx = test_find_pk(platform, platform_handle);
    if (idx < 0) {
        idx = test_find_empty();
        if (idx < 0)
            return HU_ERR_OUT_OF_MEMORY;
    }

    hu_contact_identity_t *e = &g_contact_graph_test_rows[idx].row;
    memset(e, 0, sizeof(*e));
    test_copy_str(e->contact_id, sizeof(e->contact_id), contact_id);
    test_copy_str(e->display_name, sizeof(e->display_name), display_name ? display_name : "");
    test_copy_str(e->platform, sizeof(e->platform), platform);
    test_copy_str(e->platform_handle, sizeof(e->platform_handle), platform_handle);
    e->confidence = confidence;
    g_contact_graph_test_rows[idx].used = true;
    return HU_OK;
}

hu_error_t hu_contact_graph_resolve(void *db, const char *platform, const char *platform_handle,
    char *out_contact_id, size_t out_size) {
    (void)db;
    if (!test_non_empty(platform) || !test_non_empty(platform_handle) || !out_contact_id ||
        out_size == 0)
        return HU_ERR_INVALID_ARGUMENT;

    int idx = test_find_pk(platform, platform_handle);
    if (idx < 0)
        return HU_ERR_NOT_FOUND;

    test_copy_str(out_contact_id, out_size, g_contact_graph_test_rows[idx].row.contact_id);
    return HU_OK;
}

hu_error_t hu_contact_graph_list(hu_allocator_t *alloc, void *db, const char *contact_id,
    hu_contact_identity_t **out, size_t *out_count) {
    (void)db;
    if (!alloc || !test_non_empty(contact_id) || !out || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0;

    size_t cap = 4;
    size_t count = 0;
    hu_contact_identity_t *arr =
        (hu_contact_identity_t *)alloc->alloc(alloc->ctx, cap * sizeof(hu_contact_identity_t));
    if (!arr)
        return HU_ERR_OUT_OF_MEMORY;

    for (int i = 0; i < hu_contact_graph_test_cap; i++) {
        if (!g_contact_graph_test_rows[i].used)
            continue;
        if (strcmp(g_contact_graph_test_rows[i].row.contact_id, contact_id) != 0)
            continue;
        if (count >= cap) {
            size_t new_cap = cap * 2;
            hu_contact_identity_t *n = (hu_contact_identity_t *)alloc->alloc(
                alloc->ctx, new_cap * sizeof(hu_contact_identity_t));
            if (!n) {
                alloc->free(alloc->ctx, arr, cap * sizeof(hu_contact_identity_t));
                return HU_ERR_OUT_OF_MEMORY;
            }
            memcpy(n, arr, count * sizeof(hu_contact_identity_t));
            alloc->free(alloc->ctx, arr, cap * sizeof(hu_contact_identity_t));
            arr = n;
            cap = new_cap;
        }
        arr[count] = g_contact_graph_test_rows[i].row;
        count++;
    }

    if (count == 0) {
        alloc->free(alloc->ctx, arr, cap * sizeof(hu_contact_identity_t));
        arr = NULL;
    }
    *out = arr;
    *out_count = count;
    return HU_OK;
}

hu_error_t hu_contact_graph_merge(void *db, const char *old_id, const char *new_id) {
    (void)db;
    if (!test_non_empty(old_id) || !test_non_empty(new_id))
        return HU_ERR_INVALID_ARGUMENT;

    for (int i = 0; i < hu_contact_graph_test_cap; i++) {
        if (!g_contact_graph_test_rows[i].used)
            continue;
        if (strcmp(g_contact_graph_test_rows[i].row.contact_id, old_id) == 0)
            test_copy_str(g_contact_graph_test_rows[i].row.contact_id,
                sizeof(g_contact_graph_test_rows[i].row.contact_id), new_id);
    }
    return HU_OK;
}

hu_error_t hu_contact_graph_auto_resolve(hu_allocator_t *alloc, void *db,
    const char *display_name, const char *platform_handle,
    hu_contact_identity_t **out_candidates, size_t *out_count) {
    (void)db;
    if (!alloc || !out_candidates || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_candidates = NULL;
    *out_count = 0;

    hu_contact_identity_t candidates[32];
    size_t count = 0;

    if (platform_handle && platform_handle[0]) {
        for (int i = 0; i < hu_contact_graph_test_cap && count < 32; i++) {
            if (!g_contact_graph_test_rows[i].used)
                continue;
            if (strcmp(g_contact_graph_test_rows[i].row.platform_handle, platform_handle) != 0)
                continue;
            candidates[count++] = g_contact_graph_test_rows[i].row;
        }
    }

    if (platform_handle && platform_handle[0] && count < 32) {
        char norm[32];
        size_t nlen = hu_contact_normalize_phone(platform_handle, norm, sizeof(norm));
        if (nlen >= 7) {
            for (int i = 0; i < hu_contact_graph_test_cap && count < 32; i++) {
                if (!g_contact_graph_test_rows[i].used)
                    continue;
                const char *ph = g_contact_graph_test_rows[i].row.platform_handle;
                char other_norm[32];
                size_t olen = hu_contact_normalize_phone(ph, other_norm, sizeof(other_norm));
                if (olen < 7 || olen != nlen || memcmp(norm, other_norm, nlen) != 0)
                    continue;
                const char *cid = g_contact_graph_test_rows[i].row.contact_id;
                if (test_candidate_dup(candidates, count, cid, ph))
                    continue;
                candidates[count] = g_contact_graph_test_rows[i].row;
                candidates[count].confidence *= 0.8;
                count++;
            }
        }
    }

    if (display_name && display_name[0] && count < 32) {
        for (int i = 0; i < hu_contact_graph_test_cap && count < 32; i++) {
            if (!g_contact_graph_test_rows[i].used)
                continue;
            if (!test_substring_ci(g_contact_graph_test_rows[i].row.display_name, display_name))
                continue;
            const char *cid = g_contact_graph_test_rows[i].row.contact_id;
            const char *ph = g_contact_graph_test_rows[i].row.platform_handle;
            if (test_candidate_dup(candidates, count, cid, ph))
                continue;
            candidates[count] = g_contact_graph_test_rows[i].row;
            candidates[count].confidence *= 0.5;
            count++;
        }
    }

    if (count == 0)
        return HU_OK;

    for (size_t i = 1; i < count; i++) {
        hu_contact_identity_t tmp = candidates[i];
        size_t j = i;
        while (j > 0 && candidates[j - 1].confidence < tmp.confidence) {
            candidates[j] = candidates[j - 1];
            j--;
        }
        candidates[j] = tmp;
    }

    hu_contact_identity_t *out = (hu_contact_identity_t *)alloc->alloc(
        alloc->ctx, count * sizeof(hu_contact_identity_t));
    if (!out)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(out, candidates, count * sizeof(hu_contact_identity_t));
    *out_candidates = out;
    *out_count = count;
    return HU_OK;
}

#else /* !HU_ENABLE_SQLITE && !HU_IS_TEST */

hu_error_t hu_contact_graph_init(hu_allocator_t *alloc, void *db) {
    (void)alloc;
    (void)db;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_contact_graph_link(void *db, const char *contact_id, const char *platform,
    const char *platform_handle, const char *display_name, double confidence) {
    (void)db;
    (void)contact_id;
    (void)platform;
    (void)platform_handle;
    (void)display_name;
    (void)confidence;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_contact_graph_resolve(void *db, const char *platform, const char *platform_handle,
    char *out_contact_id, size_t out_size) {
    (void)db;
    (void)platform;
    (void)platform_handle;
    if (out_contact_id && out_size > 0)
        out_contact_id[0] = '\0';
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_contact_graph_list(hu_allocator_t *alloc, void *db, const char *contact_id,
    hu_contact_identity_t **out, size_t *out_count) {
    if (out)
        *out = NULL;
    if (out_count)
        *out_count = 0;
    (void)alloc;
    (void)db;
    (void)contact_id;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_contact_graph_merge(void *db, const char *old_id, const char *new_id) {
    (void)db;
    (void)old_id;
    (void)new_id;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_contact_graph_auto_resolve(hu_allocator_t *alloc, void *db,
    const char *display_name, const char *platform_handle,
    hu_contact_identity_t **out_candidates, size_t *out_count) {
    if (!alloc || !out_candidates || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_candidates = NULL;
    *out_count = 0;
    (void)db;
    (void)display_name;
    (void)platform_handle;
    return HU_ERR_NOT_SUPPORTED;
}

#endif
