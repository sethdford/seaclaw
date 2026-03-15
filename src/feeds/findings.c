#include "human/feeds/findings.h"
#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

hu_error_t hu_findings_store(hu_allocator_t *alloc, sqlite3 *db,
                             const char *source, const char *finding,
                             const char *relevance, const char *priority,
                             const char *suggested_action) {
    (void)alloc;
    if (!db || !finding) return HU_ERR_INVALID_ARGUMENT;

    const char *sql =
        "INSERT INTO research_findings (source, finding, relevance, priority, "
        "suggested_action, status, created_at) VALUES (?, ?, ?, ?, ?, 'pending', ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;

    sqlite3_bind_text(stmt, 1, source ? source : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, finding, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, relevance ? relevance : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, priority ? priority : "MEDIUM", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, suggested_action ? suggested_action : "", -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, (int64_t)time(NULL));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_findings_get_pending(hu_allocator_t *alloc, sqlite3 *db,
                                   size_t limit,
                                   hu_research_finding_t **out, size_t *out_count) {
    if (!alloc || !db || !out || !out_count) return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0;

    const char *sql =
        "SELECT id, source, finding, relevance, priority, suggested_action, status, created_at "
        "FROM research_findings WHERE status = 'pending' ORDER BY created_at DESC LIMIT ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_int(stmt, 1, (int)limit);

    hu_research_finding_t *items = NULL;
    size_t count = 0, cap = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            size_t new_cap = cap == 0 ? 4 : cap * 2;
            hu_research_finding_t *tmp = (hu_research_finding_t *)alloc->alloc(
                alloc->ctx, new_cap * sizeof(hu_research_finding_t));
            if (!tmp) {
                if (items) alloc->free(alloc->ctx, items, cap * sizeof(hu_research_finding_t));
                sqlite3_finalize(stmt);
                return HU_ERR_OUT_OF_MEMORY;
            }
            if (items) {
                memcpy(tmp, items, count * sizeof(hu_research_finding_t));
                alloc->free(alloc->ctx, items, cap * sizeof(hu_research_finding_t));
            }
            items = tmp;
            cap = new_cap;
        }
        hu_research_finding_t *f = &items[count];
        memset(f, 0, sizeof(*f));
        f->id = sqlite3_column_int64(stmt, 0);
        const char *s;
        s = (const char *)sqlite3_column_text(stmt, 1);
        if (s) snprintf(f->source, sizeof(f->source), "%s", s);
        s = (const char *)sqlite3_column_text(stmt, 2);
        if (s) snprintf(f->finding, sizeof(f->finding), "%s", s);
        s = (const char *)sqlite3_column_text(stmt, 3);
        if (s) snprintf(f->relevance, sizeof(f->relevance), "%s", s);
        s = (const char *)sqlite3_column_text(stmt, 4);
        if (s) snprintf(f->priority, sizeof(f->priority), "%s", s);
        s = (const char *)sqlite3_column_text(stmt, 5);
        if (s) snprintf(f->suggested_action, sizeof(f->suggested_action), "%s", s);
        s = (const char *)sqlite3_column_text(stmt, 6);
        if (s) snprintf(f->status, sizeof(f->status), "%s", s);
        f->created_at = sqlite3_column_int64(stmt, 7);
        count++;
    }
    sqlite3_finalize(stmt);
    *out = items;
    *out_count = count;
    return HU_OK;
}

void hu_findings_free(hu_allocator_t *alloc, hu_research_finding_t *items, size_t count) {
    if (items && alloc)
        alloc->free(alloc->ctx, items, count * sizeof(hu_research_finding_t));
}

hu_error_t hu_findings_mark_status(sqlite3 *db, int64_t id, const char *status) {
    if (!db || !status) return HU_ERR_INVALID_ARGUMENT;
    const char *sql = "UPDATE research_findings SET status = ?, acted_at = ? WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return HU_ERR_IO;
    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, (int64_t)time(NULL));
    sqlite3_bind_int64(stmt, 3, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_findings_get_all(hu_allocator_t *alloc, sqlite3 *db,
                               size_t limit,
                               hu_research_finding_t **out, size_t *out_count) {
    if (!alloc || !db || !out || !out_count) return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0;

    const char *sql =
        "SELECT id, source, finding, relevance, priority, suggested_action, status, created_at "
        "FROM research_findings ORDER BY created_at DESC LIMIT ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_int(stmt, 1, (int)limit);

    hu_research_finding_t *items = NULL;
    size_t count = 0, cap = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            size_t new_cap = cap == 0 ? 4 : cap * 2;
            hu_research_finding_t *tmp = (hu_research_finding_t *)alloc->alloc(
                alloc->ctx, new_cap * sizeof(hu_research_finding_t));
            if (!tmp) {
                if (items) alloc->free(alloc->ctx, items, cap * sizeof(hu_research_finding_t));
                sqlite3_finalize(stmt);
                return HU_ERR_OUT_OF_MEMORY;
            }
            if (items) {
                memcpy(tmp, items, count * sizeof(hu_research_finding_t));
                alloc->free(alloc->ctx, items, cap * sizeof(hu_research_finding_t));
            }
            items = tmp;
            cap = new_cap;
        }
        hu_research_finding_t *f = &items[count];
        memset(f, 0, sizeof(*f));
        f->id = sqlite3_column_int64(stmt, 0);
        const char *s;
        s = (const char *)sqlite3_column_text(stmt, 1);
        if (s) snprintf(f->source, sizeof(f->source), "%s", s);
        s = (const char *)sqlite3_column_text(stmt, 2);
        if (s) snprintf(f->finding, sizeof(f->finding), "%s", s);
        s = (const char *)sqlite3_column_text(stmt, 3);
        if (s) snprintf(f->relevance, sizeof(f->relevance), "%s", s);
        s = (const char *)sqlite3_column_text(stmt, 4);
        if (s) snprintf(f->priority, sizeof(f->priority), "%s", s);
        s = (const char *)sqlite3_column_text(stmt, 5);
        if (s) snprintf(f->suggested_action, sizeof(f->suggested_action), "%s", s);
        s = (const char *)sqlite3_column_text(stmt, 6);
        if (s) snprintf(f->status, sizeof(f->status), "%s", s);
        f->created_at = sqlite3_column_int64(stmt, 7);
        count++;
    }
    sqlite3_finalize(stmt);
    *out = items;
    *out_count = count;
    return HU_OK;
}

hu_error_t hu_findings_parse_and_store(hu_allocator_t *alloc, sqlite3 *db,
                                       const char *agent_output, size_t output_len) {
    if (!alloc || !db || !agent_output || output_len == 0) return HU_ERR_INVALID_ARGUMENT;

    const char *p = agent_output;
    const char *end = agent_output + output_len;
    size_t stored = 0;

    while (p < end) {
        /* Find next "- **Source**:" marker */
        const char *marker = "- **Source**:";
        size_t marker_len = strlen(marker);
        const char *found = NULL;
        for (const char *s = p; s + marker_len <= end; s++) {
            if (memcmp(s, marker, marker_len) == 0) { found = s; break; }
        }
        if (!found) break;

        /* Extract fields by scanning lines */
        char source[256] = {0};
        char finding[2048] = {0};
        char relevance[256] = {0};
        char priority[16] = {0};
        char action[1024] = {0};

        const char *line = found;
        for (int field = 0; field < 5 && line < end; field++) {
            /* Skip to after ": " */
            const char *colon = NULL;
            for (const char *c = line; c < end && *c != '\n'; c++) {
                if (*c == ':' && c + 1 < end && *(c+1) == ' ') { colon = c + 2; break; }
            }
            /* Find end of line */
            const char *eol = line;
            while (eol < end && *eol != '\n') eol++;

            if (colon && colon < eol) {
                size_t vlen = (size_t)(eol - colon);
                switch (field) {
                    case 0: if (vlen < sizeof(source)) memcpy(source, colon, vlen); break;
                    case 1: if (vlen < sizeof(finding)) memcpy(finding, colon, vlen); break;
                    case 2: if (vlen < sizeof(relevance)) memcpy(relevance, colon, vlen); break;
                    case 3: if (vlen < sizeof(priority)) memcpy(priority, colon, vlen); break;
                    case 4: if (vlen < sizeof(action)) memcpy(action, colon, vlen); break;
                }
            }
            line = (eol < end) ? eol + 1 : end;
        }

        if (finding[0]) {
            hu_findings_store(alloc, db, source, finding, relevance,
                              priority[0] ? priority : "MEDIUM", action);
            stored++;
        }
        p = line;
    }

    if (stored > 0)
        fprintf(stderr, "[findings] parsed and stored %zu findings\n", stored);
    return HU_OK;
}

#endif /* HU_ENABLE_SQLITE */
