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

/* Search for a field marker like "**Source**:" or "**Source:**" and extract the value */
static const char *find_field_value(const char *start, const char *end,
                                    const char *field_name, size_t *value_len) {
    size_t fn_len = strlen(field_name);
    for (const char *s = start; s + fn_len + 4 <= end; s++) {
        /* Match: "**<field>**:" or "**<field>:**" */
        if (s[0] != '*' || s[1] != '*')
            continue;
        if (memcmp(s + 2, field_name, fn_len) != 0)
            continue;
        const char *after = s + 2 + fn_len;
        /* **Field**: or **Field:**: */
        const char *colon = NULL;
        if (after + 3 <= end && after[0] == '*' && after[1] == '*' && after[2] == ':')
            colon = after + 3;
        else if (after + 3 <= end && after[0] == ':' && after[1] == '*' && after[2] == '*')
            colon = after + 3;
        else if (after + 4 <= end && after[0] == '*' && after[1] == '*' && after[2] == ':' && after[3] == ' ')
            colon = after + 4;
        else
            continue;

        while (colon < end && *colon == ' ')
            colon++;
        /* Value runs to end of line or next "**" marker */
        const char *val_end = colon;
        while (val_end < end && *val_end != '\n') {
            if (val_end + 2 < end && val_end[0] == '*' && val_end[1] == '*' && val_end > colon + 2)
                break;
            val_end++;
        }
        /* Trim trailing whitespace and punctuation */
        while (val_end > colon && (val_end[-1] == ' ' || val_end[-1] == '\t'))
            val_end--;
        *value_len = (size_t)(val_end - colon);
        return colon;
    }
    return NULL;
}

static void copy_field(char *dst, size_t dst_cap, const char *src, size_t src_len) {
    size_t n = src_len < dst_cap - 1 ? src_len : dst_cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static const char *classify_priority(const char *text, size_t len) {
    for (size_t i = 0; i + 3 <= len; i++) {
        if ((text[i] == 'H' || text[i] == 'h') &&
            (text[i+1] == 'I' || text[i+1] == 'i') &&
            (text[i+2] == 'G' || text[i+2] == 'g'))
            return "HIGH";
        if ((text[i] == 'L' || text[i] == 'l') &&
            (text[i+1] == 'O' || text[i+1] == 'o') &&
            (text[i+2] == 'W' || text[i+2] == 'w'))
            return "LOW";
    }
    return "MEDIUM";
}

hu_error_t hu_findings_parse_and_store(hu_allocator_t *alloc, sqlite3 *db,
                                       const char *agent_output, size_t output_len) {
    if (!alloc || !db || !agent_output || output_len == 0) return HU_ERR_INVALID_ARGUMENT;

    const char *p = agent_output;
    const char *end = agent_output + output_len;
    size_t stored = 0;

    /* Strategy 1: line-by-line "- **Source**: value" format */
    while (p < end) {
        const char *marker = "- **Source**:";
        size_t marker_len = 13;
        const char *found = NULL;
        for (const char *s = p; s + marker_len <= end; s++) {
            if (memcmp(s, marker, marker_len) == 0) { found = s; break; }
        }
        if (!found) break;

        char source[256] = {0}, finding[2048] = {0};
        char relevance[256] = {0}, priority[16] = {0}, action[1024] = {0};

        const char *line = found;
        for (int field = 0; field < 5 && line < end; field++) {
            const char *colon = NULL;
            for (const char *c = line; c < end && *c != '\n'; c++) {
                if (*c == ':' && c + 1 < end && *(c+1) == ' ') { colon = c + 2; break; }
            }
            const char *eol = line;
            while (eol < end && *eol != '\n') eol++;
            if (colon && colon < eol) {
                size_t vlen = (size_t)(eol - colon);
                switch (field) {
                    case 0: copy_field(source, sizeof(source), colon, vlen); break;
                    case 1: copy_field(finding, sizeof(finding), colon, vlen); break;
                    case 2: copy_field(relevance, sizeof(relevance), colon, vlen); break;
                    case 3: copy_field(priority, sizeof(priority), colon, vlen); break;
                    case 4: copy_field(action, sizeof(action), colon, vlen); break;
                }
            }
            line = (eol < end) ? eol + 1 : end;
        }
        {
            const char *f = finding;
            while (*f == ' ' || *f == '\t' || *f == '\n' || *f == '\r') f++;
            if (*f) {
                hu_findings_store(alloc, db, source, finding, relevance,
                                  priority[0] ? priority : "MEDIUM", action);
                stored++;
            }
        }
        p = line;
    }

    /* Strategy 2: inline "**Source**: ... **Finding**: ... **Priority**: ..." within paragraphs */
    if (stored == 0) {
        p = agent_output;
        while (p < end) {
            size_t src_len = 0, find_len = 0, rel_len = 0, pri_len = 0, act_len = 0;
            const char *src_v = find_field_value(p, end, "Source", &src_len);
            if (!src_v) {
                /* Also try "Source" as part of bullet: "**<Title> (Source):**" */
                src_v = find_field_value(p, end, "Finding", &find_len);
                if (!src_v) break;
            } else {
                find_field_value(p, end, "Finding", &find_len);
            }

            const char *pri_v = find_field_value(p, end, "Priority", &pri_len);
            const char *rel_v = find_field_value(p, end, "Relevance", &rel_len);
            const char *act_v = find_field_value(p, end, "Suggested Action", &act_len);
            if (!act_v)
                act_v = find_field_value(p, end, "Action", &act_len);

            char source[256] = {0}, finding[2048] = {0};
            char relevance[256] = {0}, action[1024] = {0};

            if (src_v && src_len > 0)
                copy_field(source, sizeof(source), src_v, src_len);
            if (find_len > 0) {
                const char *fv = find_field_value(p, end, "Finding", &find_len);
                if (fv) copy_field(finding, sizeof(finding), fv, find_len);
            }
            if (rel_v && rel_len > 0)
                copy_field(relevance, sizeof(relevance), rel_v, rel_len);
            if (act_v && act_len > 0)
                copy_field(action, sizeof(action), act_v, act_len);

            const char *pri_class = "MEDIUM";
            if (pri_v && pri_len > 0)
                pri_class = classify_priority(pri_v, pri_len);

            /* Require a non-empty finding AND a priority marker to avoid false positives */
            if (finding[0] && pri_v) {
                hu_findings_store(alloc, db, source, finding, relevance, pri_class, action);
                stored++;
            }

            /* Advance past this block (find next paragraph or bullet) */
            const char *next = (pri_v && pri_len > 0) ? pri_v + pri_len : src_v + src_len;
            if (act_v && act_len > 0 && act_v + act_len > next)
                next = act_v + act_len;
            while (next < end && *next != '\n') next++;
            if (next < end) next++;
            if (next <= p) next = p + 1;
            p = next;
        }
    }

    if (stored > 0)
        fprintf(stderr, "[findings] parsed and stored %zu findings\n", stored);
    return HU_OK;
}

#endif /* HU_ENABLE_SQLITE */
