/*
 * AGI-S5 Multi-Graph Memory (MAGMA Pattern).
 * memory_nodes + memory_edges with semantic, temporal, entity, causal graph types.
 */
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory/memory_graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

#define HU_MEMORY_GRAPH_BFS_MAX 256

static int64_t now_ms(void) {
    return (int64_t)time(NULL) * 1000;
}

/* Simple content hash: first 32 bytes as hex (64 chars). If shorter, pad with sum. */
static void content_to_hash(const char *content, size_t len, char *out_hash) {
    unsigned long sum = 0;
    size_t i;
    for (i = 0; i < len && i < 32; i++)
        sum += (unsigned char)content[i];
    for (i = len; i < 32; i++)
        sum += (unsigned char)(i & 0xff);
    for (i = 0; i < 32; i++) {
        unsigned char b = i < len ? (unsigned char)content[i] : (unsigned char)(sum >> (i % 8));
        snprintf(out_hash + i * 2, 3, "%02x", b);
    }
    out_hash[64] = '\0';
}

hu_error_t hu_memory_graph_create(hu_allocator_t *alloc, sqlite3 *db, hu_memory_graph_t *out) {
    if (!alloc || !db || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->alloc = alloc;
    out->db = db;
    return hu_memory_graph_init_tables(out);
}

hu_error_t hu_memory_graph_init_tables(hu_memory_graph_t *g) {
    if (!g || !g->db)
        return HU_ERR_INVALID_ARGUMENT;

    static const char *const SCHEMA[] = {
        "CREATE TABLE IF NOT EXISTS memory_nodes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "content_hash TEXT NOT NULL,"
        "content_preview TEXT NOT NULL,"
        "created_at INTEGER NOT NULL)",
        "CREATE TABLE IF NOT EXISTS memory_edges ("
        "source_id INTEGER NOT NULL REFERENCES memory_nodes(id),"
        "target_id INTEGER NOT NULL REFERENCES memory_nodes(id),"
        "graph_type INTEGER NOT NULL,"
        "weight REAL NOT NULL DEFAULT 1.0,"
        "created_at INTEGER NOT NULL,"
        "UNIQUE(source_id, target_id, graph_type))",
        "CREATE INDEX IF NOT EXISTS idx_memory_edges_source ON memory_edges(source_id)",
        "CREATE INDEX IF NOT EXISTS idx_memory_edges_target ON memory_edges(target_id)",
        "CREATE INDEX IF NOT EXISTS idx_memory_edges_type ON memory_edges(graph_type)",
        NULL,
    };

    for (size_t i = 0; SCHEMA[i] != NULL; i++) {
        char *errmsg = NULL;
        int rc = sqlite3_exec(g->db, SCHEMA[i], NULL, NULL, &errmsg);
        if (rc != SQLITE_OK) {
            if (errmsg)
                sqlite3_free(errmsg);
            return HU_ERR_IO;
        }
    }
    return HU_OK;
}

void hu_memory_graph_deinit(hu_memory_graph_t *g) {
    if (!g)
        return;
    g->db = NULL;
    g->alloc = NULL;
}

hu_error_t hu_memory_graph_add_node(hu_memory_graph_t *g, const char *content, size_t content_len,
                                    int64_t *out_id) {
    if (!g || !g->db || !content || !out_id)
        return HU_ERR_INVALID_ARGUMENT;

    char hash[65];
    content_to_hash(content, content_len, hash);

    size_t preview_len = content_len > 255 ? 255 : content_len;
    char preview[256];
    memcpy(preview, content, preview_len);
    preview[preview_len] = '\0';

    const char *sql =
        "INSERT INTO memory_nodes (content_hash, content_preview, created_at) VALUES (?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    sqlite3_bind_text(stmt, 1, hash, 64, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, preview, (int)preview_len, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, now_ms());

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return HU_ERR_IO;

    *out_id = sqlite3_last_insert_rowid(g->db);
    return HU_OK;
}

hu_error_t hu_memory_graph_add_edge(hu_memory_graph_t *g, int64_t source, int64_t target,
                                    hu_memory_graph_type_t type, double weight) {
    if (!g || !g->db)
        return HU_ERR_INVALID_ARGUMENT;

    const char *sql =
        "INSERT OR IGNORE INTO memory_edges (source_id, target_id, graph_type, weight, created_at) "
        "VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    sqlite3_bind_int64(stmt, 1, source);
    sqlite3_bind_int64(stmt, 2, target);
    sqlite3_bind_int(stmt, 3, (int)type);
    sqlite3_bind_double(stmt, 4, weight);
    sqlite3_bind_int64(stmt, 5, now_ms());

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_memory_graph_traverse(hu_memory_graph_t *g, int64_t start_node,
                                    hu_memory_graph_type_t type, int max_hops,
                                    hu_memory_node_t *results, size_t max_results,
                                    size_t *out_count) {
    if (!g || !g->db || !results || !out_count)
        return HU_ERR_INVALID_ARGUMENT;

    *out_count = 0;
    if (max_results == 0)
        return HU_OK;

    int64_t queue[HU_MEMORY_GRAPH_BFS_MAX];
    size_t q_head = 0, q_tail = 0;
    queue[q_tail++] = start_node;
    if (q_tail >= HU_MEMORY_GRAPH_BFS_MAX)
        q_tail = 0;

    int64_t visited[HU_MEMORY_GRAPH_BFS_MAX];
    size_t visited_count = 0;
    visited[visited_count++] = start_node;

    const char *neighbor_sql =
        "SELECT target_id FROM memory_edges WHERE source_id = ? AND graph_type = ?";
    sqlite3_stmt *stmt = NULL;

    for (int hop = 0; hop < max_hops && q_head != q_tail && *out_count < max_results; hop++) {
        size_t level_size =
            (q_tail >= q_head) ? (q_tail - q_head) : (HU_MEMORY_GRAPH_BFS_MAX - q_head + q_tail);

        for (size_t l = 0; l < level_size && *out_count < max_results; l++) {
            int64_t cur = queue[q_head++];
            if (q_head >= HU_MEMORY_GRAPH_BFS_MAX)
                q_head = 0;

            if (sqlite3_prepare_v2(g->db, neighbor_sql, -1, &stmt, NULL) != SQLITE_OK)
                return HU_ERR_IO;
            sqlite3_bind_int64(stmt, 1, cur);
            sqlite3_bind_int(stmt, 2, (int)type);

            while (sqlite3_step(stmt) == SQLITE_ROW && *out_count < max_results) {
                int64_t nid = sqlite3_column_int64(stmt, 0);
                bool already = false;
                for (size_t v = 0; v < visited_count; v++) {
                    if (visited[v] == nid) {
                        already = true;
                        break;
                    }
                }
                if (already)
                    continue;

                if (visited_count < HU_MEMORY_GRAPH_BFS_MAX)
                    visited[visited_count++] = nid;
                if (q_tail < HU_MEMORY_GRAPH_BFS_MAX || q_head > 0) {
                    queue[q_tail++] = nid;
                    if (q_tail >= HU_MEMORY_GRAPH_BFS_MAX)
                        q_tail = 0;
                }

                const char *node_sql =
                    "SELECT id, content_hash, content_preview FROM memory_nodes WHERE id = ?";
                sqlite3_stmt *nstmt = NULL;
                if (sqlite3_prepare_v2(g->db, node_sql, -1, &nstmt, NULL) != SQLITE_OK) {
                    sqlite3_finalize(stmt);
                    return HU_ERR_IO;
                }
                sqlite3_bind_int64(nstmt, 1, nid);
                if (sqlite3_step(nstmt) == SQLITE_ROW) {
                    hu_memory_node_t *n = &results[*out_count];
                    memset(n, 0, sizeof(*n));
                    n->id = sqlite3_column_int64(nstmt, 0);
                    const char *h = (const char *)sqlite3_column_text(nstmt, 1);
                    if (h)
                        memcpy(n->content_hash, h, 64);
                    n->content_hash[64] = '\0';
                    const char *p = (const char *)sqlite3_column_text(nstmt, 2);
                    if (p) {
                        size_t plen = (size_t)sqlite3_column_bytes(nstmt, 2);
                        if (plen > 255)
                            plen = 255;
                        memcpy(n->content_preview, p, plen);
                        n->content_preview[plen] = '\0';
                        n->preview_len = plen;
                    }
                    n->type = type;
                    (*out_count)++;
                }
                sqlite3_finalize(nstmt);
            }
            sqlite3_finalize(stmt);
        }
    }

    return HU_OK;
}

hu_error_t hu_memory_graph_find_bridges(hu_memory_graph_t *g, int64_t node_a, int64_t node_b,
                                        hu_memory_node_t *bridges, size_t max_bridges,
                                        size_t *out_count) {
    if (!g || !g->db || !bridges || !out_count)
        return HU_ERR_INVALID_ARGUMENT;

    *out_count = 0;

    int64_t from_a[HU_MEMORY_GRAPH_BFS_MAX];
    size_t from_a_count = 0;
    int64_t from_b[HU_MEMORY_GRAPH_BFS_MAX];
    size_t from_b_count = 0;

    int64_t qa[HU_MEMORY_GRAPH_BFS_MAX];
    size_t qa_head = 0, qa_tail = 0;
    qa[qa_tail++] = node_a;
    from_a[from_a_count++] = node_a;

    int64_t qb[HU_MEMORY_GRAPH_BFS_MAX];
    size_t qb_head = 0, qb_tail = 0;
    qb[qb_tail++] = node_b;
    from_b[from_b_count++] = node_b;

    const char *neighbor_sql = "SELECT target_id FROM memory_edges WHERE source_id = ?";

    for (int hop = 0; hop < 3; hop++) {
        size_t na =
            (qa_tail >= qa_head) ? (qa_tail - qa_head) : (HU_MEMORY_GRAPH_BFS_MAX - qa_head + qa_tail);
        for (size_t i = 0; i < na && from_a_count < HU_MEMORY_GRAPH_BFS_MAX; i++) {
            int64_t cur = qa[qa_head++];
            if (qa_head >= HU_MEMORY_GRAPH_BFS_MAX)
                qa_head = 0;

            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(g->db, neighbor_sql, -1, &stmt, NULL) != SQLITE_OK)
                return HU_ERR_IO;
            sqlite3_bind_int64(stmt, 1, cur);
            while (sqlite3_step(stmt) == SQLITE_ROW && from_a_count < HU_MEMORY_GRAPH_BFS_MAX) {
                int64_t nid = sqlite3_column_int64(stmt, 0);
                bool dup = false;
                for (size_t v = 0; v < from_a_count; v++) {
                    if (from_a[v] == nid) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) {
                    from_a[from_a_count++] = nid;
                    qa[qa_tail++] = nid;
                    if (qa_tail >= HU_MEMORY_GRAPH_BFS_MAX)
                        qa_tail = 0;
                }
            }
            sqlite3_finalize(stmt);
        }

        size_t nb =
            (qb_tail >= qb_head) ? (qb_tail - qb_head) : (HU_MEMORY_GRAPH_BFS_MAX - qb_head + qb_tail);
        for (size_t i = 0; i < nb && from_b_count < HU_MEMORY_GRAPH_BFS_MAX; i++) {
            int64_t cur = qb[qb_head++];
            if (qb_head >= HU_MEMORY_GRAPH_BFS_MAX)
                qb_head = 0;

            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(g->db, neighbor_sql, -1, &stmt, NULL) != SQLITE_OK)
                return HU_ERR_IO;
            sqlite3_bind_int64(stmt, 1, cur);
            while (sqlite3_step(stmt) == SQLITE_ROW && from_b_count < HU_MEMORY_GRAPH_BFS_MAX) {
                int64_t nid = sqlite3_column_int64(stmt, 0);
                bool dup = false;
                for (size_t v = 0; v < from_b_count; v++) {
                    if (from_b[v] == nid) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) {
                    from_b[from_b_count++] = nid;
                    qb[qb_tail++] = nid;
                    if (qb_tail >= HU_MEMORY_GRAPH_BFS_MAX)
                        qb_tail = 0;
                }
            }
            sqlite3_finalize(stmt);
        }
    }

    for (size_t ia = 0; ia < from_a_count && *out_count < max_bridges; ia++) {
        int64_t x = from_a[ia];
        if (x == node_a || x == node_b)
            continue;
        for (size_t ib = 0; ib < from_b_count; ib++) {
            if (from_b[ib] == x) {
                const char *node_sql =
                    "SELECT id, content_hash, content_preview FROM memory_nodes WHERE id = ?";
                sqlite3_stmt *nstmt = NULL;
                if (sqlite3_prepare_v2(g->db, node_sql, -1, &nstmt, NULL) != SQLITE_OK)
                    return HU_ERR_IO;
                sqlite3_bind_int64(nstmt, 1, x);
                if (sqlite3_step(nstmt) == SQLITE_ROW) {
                    hu_memory_node_t *n = &bridges[*out_count];
                    memset(n, 0, sizeof(*n));
                    n->id = sqlite3_column_int64(nstmt, 0);
                    const char *h = (const char *)sqlite3_column_text(nstmt, 1);
                    if (h)
                        memcpy(n->content_hash, h, 64);
                    n->content_hash[64] = '\0';
                    const char *p = (const char *)sqlite3_column_text(nstmt, 2);
                    if (p) {
                        size_t plen = (size_t)sqlite3_column_bytes(nstmt, 2);
                        if (plen > 255)
                            plen = 255;
                        memcpy(n->content_preview, p, plen);
                        n->content_preview[plen] = '\0';
                        n->preview_len = plen;
                    }
                    (*out_count)++;
                }
                sqlite3_finalize(nstmt);
                break;
            }
        }
    }

    return HU_OK;
}

const char *hu_memory_graph_type_name(hu_memory_graph_type_t type) {
    switch (type) {
    case HU_GRAPH_SEMANTIC:
        return "semantic";
    case HU_GRAPH_TEMPORAL:
        return "temporal";
    case HU_GRAPH_ENTITY:
        return "entity";
    case HU_GRAPH_CAUSAL:
        return "causal";
    default:
        return "unknown";
    }
}

#endif /* HU_ENABLE_SQLITE */
