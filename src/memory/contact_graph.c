#include "human/memory/contact_graph.h"
#include "human/core/error.h"
#include <string.h>

#ifdef HU_ENABLE_SQLITE

#include <sqlite3.h>

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
    (void)alloc;
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

#endif
