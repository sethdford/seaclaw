/* Qdrant / pgvector backends implementing human/memory/vector.h (retrieval path). */

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/http.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/memory/vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HU_ENABLE_PGVECTOR)
#include <libpq-fe.h>
#endif

/* ─── Qdrant (HTTP) ───────────────────────────────────────────────────── */

typedef struct qdr_ret_ctx {
    hu_allocator_t *alloc;
    char *url;
    char *api_key;
    char *collection;
    size_t dimensions;
} qdr_ret_ctx_t;

#if HU_IS_TEST
static hu_error_t qdr_insert(void *ctx, hu_allocator_t *alloc, const char *id, size_t id_len,
                             const hu_embedding_t *embedding, const char *content,
                             size_t content_len) {
    (void)ctx;
    (void)alloc;
    (void)id;
    (void)id_len;
    (void)embedding;
    (void)content;
    (void)content_len;
    return HU_OK;
}

static hu_error_t qdr_search(void *ctx, hu_allocator_t *alloc, const hu_embedding_t *query,
                             size_t limit, hu_vector_entry_t **out, size_t *out_count) {
    (void)ctx;
    (void)alloc;
    (void)query;
    (void)limit;
    *out = NULL;
    *out_count = 0;
    return HU_OK;
}

static hu_error_t qdr_remove(void *ctx, const char *id, size_t id_len) {
    (void)ctx;
    (void)id;
    (void)id_len;
    return HU_OK;
}

static size_t qdr_count(void *ctx) {
    (void)ctx;
    return 0;
}
#else
static size_t json_escape_string(const char *in, size_t in_len, char *out, size_t out_cap) {
    size_t o = 0;
    if (out_cap < 3)
        return 0;
    out[o++] = '"';
    for (size_t i = 0; i < in_len && o + 2 < out_cap; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') {
            if (o + 2 >= out_cap)
                break;
            out[o++] = '\\';
            out[o++] = (char)c;
        } else if (c >= 32 && c != 127) {
            out[o++] = (char)c;
        }
    }
    if (o + 1 >= out_cap)
        o = out_cap - 2;
    out[o++] = '"';
    out[o] = '\0';
    return o;
}

static hu_error_t qdr_insert(void *ctx, hu_allocator_t *alloc, const char *id, size_t id_len,
                             const hu_embedding_t *embedding, const char *content,
                             size_t content_len) {
    qdr_ret_ctx_t *q = (qdr_ret_ctx_t *)ctx;
    if (!q || !alloc || !id || !embedding || !embedding->values)
        return HU_ERR_INVALID_ARGUMENT;

    char url[512];
    if ((int)snprintf(url, sizeof(url), "%s/collections/%s/points?wait=true", q->url,
                      q->collection) >= (int)sizeof(url))
        return HU_ERR_INVALID_ARGUMENT;

    size_t body_cap = 512 + id_len * 2 + content_len * 2 + embedding->dim * 24;
    if (body_cap > 1024 * 1024)
        body_cap = 1024 * 1024;
    char *body = (char *)alloc->alloc(alloc->ctx, body_cap);
    if (!body)
        return HU_ERR_OUT_OF_MEMORY;

    size_t pos = hu_buf_appendf(body, body_cap, 0, "{\"points\":[{\"id\":\"%.*s\",\"vector\":[",
                                (int)id_len, id);
    if (pos >= body_cap - 1) {
        alloc->free(alloc->ctx, body, body_cap);
        return HU_ERR_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < embedding->dim && pos + 32 < body_cap; i++) {
        pos = hu_buf_appendf(body, body_cap, pos, i ? ",%f" : "%f",
                             (double)embedding->values[i]);
    }
    pos = hu_buf_appendf(body, body_cap, pos, "],\"payload\":{\"key\":\"%.*s\"", (int)id_len, id);
    if (pos >= body_cap - 1) {
        alloc->free(alloc->ctx, body, body_cap);
        return HU_ERR_INVALID_ARGUMENT;
    }
    if (content && content_len > 0 && pos + 16 < body_cap) {
        body[pos++] = ',';
        memcpy(body + pos, "\"content\":", 10);
        pos += 10;
        pos += json_escape_string(content, content_len, body + pos, body_cap - pos);
    }
    pos = hu_buf_appendf(body, body_cap, pos, "}}]}");

    hu_http_response_t resp = {0};
    const char *auth = q->api_key && q->api_key[0] ? q->api_key : NULL;
    hu_error_t err = hu_http_post_json(alloc, url, auth, body, pos, &resp);
    alloc->free(alloc->ctx, body, body_cap);
    if (err != HU_OK)
        return err;
    long st = resp.status_code;
    hu_http_response_free(alloc, &resp);
    return st == 200 ? HU_OK : HU_ERR_MEMORY_BACKEND;
}

static hu_error_t qdr_search(void *ctx, hu_allocator_t *alloc, const hu_embedding_t *query,
                             size_t limit, hu_vector_entry_t **out, size_t *out_count) {
    qdr_ret_ctx_t *q = (qdr_ret_ctx_t *)ctx;
    *out = NULL;
    *out_count = 0;
    if (!q || !alloc || !query || !query->values)
        return HU_ERR_INVALID_ARGUMENT;

    char url[512];
    if ((int)snprintf(url, sizeof(url), "%s/collections/%s/points/search", q->url,
                      q->collection) >= (int)sizeof(url))
        return HU_ERR_INVALID_ARGUMENT;

    size_t body_cap = 256 + query->dim * 24;
    char *body = (char *)alloc->alloc(alloc->ctx, body_cap);
    if (!body)
        return HU_ERR_OUT_OF_MEMORY;
    size_t pos = hu_buf_appendf(body, body_cap, 0, "{\"vector\":[");
    if (pos >= body_cap - 1) {
        alloc->free(alloc->ctx, body, body_cap);
        return HU_ERR_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < query->dim && pos + 32 < body_cap; i++) {
        pos = hu_buf_appendf(body, body_cap, pos, i ? ",%f" : "%f", (double)query->values[i]);
    }
    pos = hu_buf_appendf(body, body_cap, pos, "],\"limit\":%zu,\"with_payload\":true}",
                        limit > 0 ? limit : 10);

    hu_http_response_t resp = {0};
    const char *auth = q->api_key && q->api_key[0] ? q->api_key : NULL;
    hu_error_t err = hu_http_post_json(alloc, url, auth, body, pos, &resp);
    alloc->free(alloc->ctx, body, body_cap);
    if (err != HU_OK)
        return err;
    if (resp.status_code != 200 || !resp.body || resp.body_len == 0) {
        hu_http_response_free(alloc, &resp);
        return HU_ERR_MEMORY_BACKEND;
    }

    hu_json_value_t *parsed = NULL;
    hu_error_t pe = hu_json_parse(alloc, resp.body, resp.body_len, &parsed);
    hu_http_response_free(alloc, &resp);
    if (pe != HU_OK || !parsed)
        return HU_ERR_MEMORY_BACKEND;

    hu_json_value_t *result_arr = hu_json_object_get(parsed, "result");
    if (!result_arr || result_arr->type != HU_JSON_ARRAY || result_arr->data.array.len == 0) {
        hu_json_free(alloc, parsed);
        return HU_OK;
    }

    size_t n = result_arr->data.array.len;
    hu_vector_entry_t *arr =
        (hu_vector_entry_t *)alloc->alloc(alloc->ctx, n * sizeof(hu_vector_entry_t));
    if (!arr) {
        hu_json_free(alloc, parsed);
        return HU_ERR_OUT_OF_MEMORY;
    }
    memset(arr, 0, n * sizeof(hu_vector_entry_t));

    size_t out_i = 0;
    for (size_t i = 0; i < n; i++) {
        hu_json_value_t *item = result_arr->data.array.items[i];
        if (!item || item->type != HU_JSON_OBJECT)
            continue;
        float score = (float)hu_json_get_number(item, "score", 0.0);
        hu_json_value_t *payload = hu_json_object_get(item, "payload");
        const char *key = NULL;
        const char *ct = NULL;
        if (payload && payload->type == HU_JSON_OBJECT) {
            key = hu_json_get_string(payload, "key");
            ct = hu_json_get_string(payload, "content");
        }
        if (!key)
            key = hu_json_get_string(item, "id");
        if (!key)
            key = "";
        size_t id_l = strlen(key);
        char *id_c = (char *)alloc->alloc(alloc->ctx, id_l + 1);
        if (!id_c) {
            for (size_t j = 0; j < out_i; j++) {
                if (arr[j].id)
                    alloc->free(alloc->ctx, (void *)arr[j].id, arr[j].id_len + 1);
                if (arr[j].embedding.values)
                    alloc->free(alloc->ctx, arr[j].embedding.values,
                                arr[j].embedding.dim * sizeof(float));
                if (arr[j].content)
                    alloc->free(alloc->ctx, (void *)arr[j].content, arr[j].content_len + 1);
            }
            alloc->free(alloc->ctx, arr, n * sizeof(hu_vector_entry_t));
            hu_json_free(alloc, parsed);
            return HU_ERR_OUT_OF_MEMORY;
        }
        memcpy(id_c, key, id_l + 1);
        arr[out_i].id = id_c;
        arr[out_i].id_len = id_l;
        arr[out_i].score = score;
        arr[out_i].embedding.values = NULL;
        arr[out_i].embedding.dim = 0;
        if (ct && ct[0]) {
            size_t cl = strlen(ct);
            char *cc = (char *)alloc->alloc(alloc->ctx, cl + 1);
            if (!cc) {
                alloc->free(alloc->ctx, id_c, id_l + 1);
                for (size_t j = 0; j < out_i; j++) {
                    if (arr[j].id)
                        alloc->free(alloc->ctx, (void *)arr[j].id, arr[j].id_len + 1);
                    if (arr[j].content)
                        alloc->free(alloc->ctx, (void *)arr[j].content, arr[j].content_len + 1);
                }
                alloc->free(alloc->ctx, arr, n * sizeof(hu_vector_entry_t));
                hu_json_free(alloc, parsed);
                return HU_ERR_OUT_OF_MEMORY;
            }
            memcpy(cc, ct, cl + 1);
            arr[out_i].content = cc;
            arr[out_i].content_len = cl;
        }
        out_i++;
    }
    hu_json_free(alloc, parsed);
    if (out_i == 0) {
        alloc->free(alloc->ctx, arr, n * sizeof(hu_vector_entry_t));
        return HU_OK;
    }
    *out = arr;
    *out_count = out_i;
    return HU_OK;
}

static hu_error_t qdr_remove(void *ctx, const char *id, size_t id_len) {
    qdr_ret_ctx_t *q = (qdr_ret_ctx_t *)ctx;
    if (!q || !id)
        return HU_ERR_INVALID_ARGUMENT;
    hu_allocator_t *alloc = q->alloc;
    char url[512];
    snprintf(url, sizeof(url), "%s/collections/%s/points/delete?wait=true", q->url, q->collection);
    char body[2048];
    char id_esc[512];
    size_t ie = 0;
    for (size_t i = 0; i < id_len && ie + 2 < sizeof(id_esc); i++) {
        char c = id[i];
        if (c == '"' || c == '\\') {
            id_esc[ie++] = '\\';
            id_esc[ie++] = c;
        } else if ((unsigned char)c >= 32)
            id_esc[ie++] = c;
    }
    id_esc[ie] = '\0';
    int bn = snprintf(body, sizeof(body),
                      "{\"filter\":{\"must\":[{\"key\":\"key\",\"match\":{\"value\":\"%s\"}}]}}",
                      id_esc);
    if (bn <= 0 || (size_t)bn >= sizeof(body))
        return HU_ERR_INVALID_ARGUMENT;
    hu_http_response_t resp = {0};
    const char *auth = q->api_key && q->api_key[0] ? q->api_key : NULL;
    hu_error_t err = hu_http_post_json(alloc, url, auth, body, (size_t)bn, &resp);
    long status = resp.status_code;
    hu_http_response_free(alloc, &resp);
    return err == HU_OK && status == 200 ? HU_OK : HU_ERR_MEMORY_BACKEND;
}

static size_t qdr_count(void *ctx) {
    qdr_ret_ctx_t *q = (qdr_ret_ctx_t *)ctx;
    if (!q || !q->alloc)
        return 0;
    hu_allocator_t *alloc = q->alloc;
    char url[512];
    snprintf(url, sizeof(url), "%s/collections/%s/points/count", q->url, q->collection);
    const char *req = "{\"exact\":true}";
    hu_http_response_t resp = {0};
    const char *auth = q->api_key && q->api_key[0] ? q->api_key : NULL;
    hu_error_t err = hu_http_post_json(alloc, url, auth, req, strlen(req), &resp);
    if (err != HU_OK || resp.status_code != 200 || !resp.body) {
        hu_http_response_free(alloc, &resp);
        return 0;
    }
    hu_json_value_t *parsed = NULL;
    err = hu_json_parse(alloc, resp.body, resp.body_len, &parsed);
    hu_http_response_free(alloc, &resp);
    if (err != HU_OK || !parsed)
        return 0;
    hu_json_value_t *ro = hu_json_object_get(parsed, "result");
    size_t n = 0;
    if (ro && ro->type == HU_JSON_OBJECT)
        n = (size_t)hu_json_get_number(ro, "count", 0);
    hu_json_free(alloc, parsed);
    return n;
}
#endif /* !HU_IS_TEST */

static void qdr_deinit(void *ctx, hu_allocator_t *alloc) {
    qdr_ret_ctx_t *q = (qdr_ret_ctx_t *)ctx;
    if (!q || !alloc)
        return;
    if (q->url)
        alloc->free(alloc->ctx, q->url, strlen(q->url) + 1);
    if (q->api_key)
        alloc->free(alloc->ctx, q->api_key, strlen(q->api_key) + 1);
    if (q->collection)
        alloc->free(alloc->ctx, q->collection, strlen(q->collection) + 1);
    alloc->free(alloc->ctx, q, sizeof(qdr_ret_ctx_t));
}

static const hu_vector_store_vtable_t qdr_ret_vtable = {
    .insert = qdr_insert,
    .search = qdr_search,
    .remove = qdr_remove,
    .count = qdr_count,
    .deinit = qdr_deinit,
};

hu_vector_store_t hu_vector_store_qdrant_retrieval_create(hu_allocator_t *alloc, const char *url,
                                                            const char *api_key,
                                                            const char *collection,
                                                            size_t dimensions) {
    hu_vector_store_t s = {.ctx = NULL, .vtable = NULL};
    if (!alloc || !url || !url[0] || !collection || !collection[0])
        return s;
    qdr_ret_ctx_t *q = (qdr_ret_ctx_t *)alloc->alloc(alloc->ctx, sizeof(qdr_ret_ctx_t));
    if (!q)
        return s;
    memset(q, 0, sizeof(*q));
    q->alloc = alloc;
    q->url = hu_strdup(alloc, url);
    q->api_key = (api_key && api_key[0]) ? hu_strdup(alloc, api_key) : NULL;
    q->collection = hu_strdup(alloc, collection);
    q->dimensions = dimensions > 0 ? dimensions : HU_EMBEDDING_DIM;
    if (!q->url || !q->collection) {
        qdr_deinit(q, alloc);
        return s;
    }
    (void)q->dimensions; /* collection must match embedding dim at runtime */
    s.ctx = q;
    s.vtable = &qdr_ret_vtable;
    return s;
}

/* ─── pgvector (libpq) — vector.h vtable ─────────────────────────────── */

#if defined(HU_ENABLE_PGVECTOR) && !HU_IS_TEST

static bool pgv_safe_id(const char *id) {
    if (!id || !id[0])
        return false;
    for (const char *p = id; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
              *p == '_'))
            return false;
    }
    return true;
}

static bool pgv_ensure_table(PGconn *conn, const char *table_name, size_t dimensions) {
    if (!conn || !table_name || dimensions == 0)
        return false;
    PGresult *res = PQexec(conn, "CREATE EXTENSION IF NOT EXISTS vector");
    if (res)
        PQclear(res);
    char sql[512];
    int n = snprintf(sql, sizeof(sql),
                     "CREATE TABLE IF NOT EXISTS %s (key TEXT PRIMARY KEY, embedding vector(%zu), "
                     "metadata TEXT, updated_at TIMESTAMPTZ DEFAULT now())",
                     table_name, dimensions);
    if (n < 0 || n >= (int)sizeof(sql))
        return false;
    res = PQexec(conn, sql);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res)
            PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

typedef struct pgv_ret_ctx {
    hu_allocator_t *alloc;
    PGconn *conn;
    char *connection_url;
    char *table_name;
    size_t dimensions;
} pgv_ret_ctx_t;

static hu_error_t pgv_insert(void *ctx, hu_allocator_t *alloc, const char *id, size_t id_len,
                             const hu_embedding_t *embedding, const char *content,
                             size_t content_len) {
    pgv_ret_ctx_t *p = (pgv_ret_ctx_t *)ctx;
    if (!p || !p->conn || !alloc || !id || !embedding || !embedding->values)
        return HU_ERR_INVALID_ARGUMENT;
    char *key_z = hu_strndup(alloc, id, id_len);
    if (!key_z)
        return HU_ERR_OUT_OF_MEMORY;
    char vec_buf[4096];
    size_t pos = hu_buf_appendf(vec_buf, sizeof(vec_buf), 0, "[");
    for (size_t i = 0; i < embedding->dim && pos < sizeof(vec_buf) - 32; i++) {
        pos = hu_buf_appendf(vec_buf, sizeof(vec_buf), pos, i ? ",%f" : "%f",
                             (double)embedding->values[i]);
    }
    pos = hu_buf_appendf(vec_buf, sizeof(vec_buf), pos, "]");
    char *meta_z = NULL;
    if (content && content_len > 0) {
        meta_z = hu_strndup(alloc, content, content_len);
        if (!meta_z) {
            alloc->free(alloc->ctx, key_z, id_len + 1);
            return HU_ERR_OUT_OF_MEMORY;
        }
    }
    char sql[8192];
    int slen = snprintf(sql, sizeof(sql),
                        "INSERT INTO %s (key, embedding, metadata, updated_at) "
                        "VALUES ($1, $2::vector, $3, now()) "
                        "ON CONFLICT (key) DO UPDATE SET embedding = $2::vector, metadata = $3, "
                        "updated_at = now()",
                        p->table_name);
    if (slen >= (int)sizeof(sql) || slen < 0) {
        if (meta_z)
            alloc->free(alloc->ctx, meta_z, content_len + 1);
        alloc->free(alloc->ctx, key_z, id_len + 1);
        return HU_ERR_INVALID_ARGUMENT;
    }
    const char *params[3] = {key_z, vec_buf, meta_z ? meta_z : NULL};
    PGresult *res = PQexecParams(p->conn, sql, 3, NULL, params, NULL, NULL, 0);
    if (meta_z)
        alloc->free(alloc->ctx, meta_z, content_len + 1);
    alloc->free(alloc->ctx, key_z, id_len + 1);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res)
            PQclear(res);
        return HU_ERR_MEMORY_BACKEND;
    }
    PQclear(res);
    return HU_OK;
}

static hu_error_t pgv_search(void *ctx, hu_allocator_t *alloc, const hu_embedding_t *query,
                             size_t limit, hu_vector_entry_t **out, size_t *out_count) {
    pgv_ret_ctx_t *p = (pgv_ret_ctx_t *)ctx;
    *out = NULL;
    *out_count = 0;
    if (!p || !p->conn || !alloc || !query || !query->values)
        return HU_ERR_INVALID_ARGUMENT;
    char vec_buf[4096];
    size_t pos = hu_buf_appendf(vec_buf, sizeof(vec_buf), 0, "[");
    for (size_t i = 0; i < query->dim && pos < sizeof(vec_buf) - 32; i++) {
        pos = hu_buf_appendf(vec_buf, sizeof(vec_buf), pos, i ? ",%f" : "%f",
                             (double)query->values[i]);
    }
    pos = hu_buf_appendf(vec_buf, sizeof(vec_buf), pos, "]");
    size_t lim = limit > 0 ? limit : 10;
    if (lim > 10000)
        lim = 10000;
    char sql[8192];
    int slen = snprintf(sql, sizeof(sql),
                        "SELECT key, 1 - (embedding <=> $1::vector) AS sim, metadata FROM %s "
                        "ORDER BY embedding <=> $1::vector LIMIT %zu",
                        p->table_name, lim);
    if (slen >= (int)sizeof(sql) || slen < 0)
        return HU_ERR_INVALID_ARGUMENT;
    const char *params[1] = {vec_buf};
    PGresult *res = PQexecParams(p->conn, sql, 1, NULL, params, NULL, NULL, 0);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        if (res)
            PQclear(res);
        return HU_ERR_MEMORY_BACKEND;
    }
    int nrows = PQntuples(res);
    if (nrows == 0) {
        PQclear(res);
        return HU_OK;
    }
    hu_vector_entry_t *arr =
        (hu_vector_entry_t *)alloc->alloc(alloc->ctx, (size_t)nrows * sizeof(hu_vector_entry_t));
    if (!arr) {
        PQclear(res);
        return HU_ERR_OUT_OF_MEMORY;
    }
    memset(arr, 0, (size_t)nrows * sizeof(hu_vector_entry_t));
    for (int i = 0; i < nrows; i++) {
        const char *key = PQgetvalue(res, i, 0);
        const char *sim = PQgetvalue(res, i, 1);
        const char *meta = PQgetvalue(res, i, 2);
        if (key) {
            size_t kl = strlen(key);
            char *kc = (char *)alloc->alloc(alloc->ctx, kl + 1);
            if (!kc) {
                for (int j = 0; j < i; j++) {
                    if (arr[j].id)
                        alloc->free(alloc->ctx, (void *)arr[j].id, arr[j].id_len + 1);
                    if (arr[j].content)
                        alloc->free(alloc->ctx, (void *)arr[j].content, arr[j].content_len + 1);
                }
                alloc->free(alloc->ctx, arr, (size_t)nrows * sizeof(hu_vector_entry_t));
                PQclear(res);
                return HU_ERR_OUT_OF_MEMORY;
            }
            memcpy(kc, key, kl + 1);
            arr[i].id = kc;
            arr[i].id_len = kl;
        }
        arr[i].score = (float)atof(sim ? sim : "0");
        if (meta && meta[0]) {
            size_t ml = strlen(meta);
            char *mc = (char *)alloc->alloc(alloc->ctx, ml + 1);
            if (!mc) {
                if (arr[i].id)
                    alloc->free(alloc->ctx, (void *)arr[i].id, arr[i].id_len + 1);
                for (int j = 0; j < i; j++) {
                    if (arr[j].id)
                        alloc->free(alloc->ctx, (void *)arr[j].id, arr[j].id_len + 1);
                    if (arr[j].content)
                        alloc->free(alloc->ctx, (void *)arr[j].content, arr[j].content_len + 1);
                }
                alloc->free(alloc->ctx, arr, (size_t)nrows * sizeof(hu_vector_entry_t));
                PQclear(res);
                return HU_ERR_OUT_OF_MEMORY;
            }
            memcpy(mc, meta, ml + 1);
            arr[i].content = mc;
            arr[i].content_len = ml;
        }
    }
    PQclear(res);
    *out = arr;
    *out_count = (size_t)nrows;
    return HU_OK;
}

static hu_error_t pgv_remove(void *ctx, const char *id, size_t id_len) {
    pgv_ret_ctx_t *p = (pgv_ret_ctx_t *)ctx;
    if (!p || !p->conn || !id)
        return HU_ERR_INVALID_ARGUMENT;
    hu_allocator_t *alloc = p->alloc;
    char *key_z = hu_strndup(alloc, id, id_len);
    if (!key_z)
        return HU_ERR_OUT_OF_MEMORY;
    char sql[512];
    snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE key = $1", p->table_name);
    const char *params[1] = {key_z};
    PGresult *res = PQexecParams(p->conn, sql, 1, NULL, params, NULL, NULL, 0);
    alloc->free(alloc->ctx, key_z, id_len + 1);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res)
            PQclear(res);
        return HU_ERR_MEMORY_BACKEND;
    }
    PQclear(res);
    return HU_OK;
}

static size_t pgv_count(void *ctx) {
    pgv_ret_ctx_t *p = (pgv_ret_ctx_t *)ctx;
    if (!p || !p->conn)
        return 0;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", p->table_name);
    PGresult *res = PQexec(p->conn, sql);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        if (res)
            PQclear(res);
        return 0;
    }
    const char *val = PQgetvalue(res, 0, 0);
    size_t n = val ? (size_t)atoll(val) : 0;
    PQclear(res);
    return n;
}

static void pgv_deinit(void *ctx, hu_allocator_t *alloc) {
    pgv_ret_ctx_t *p = (pgv_ret_ctx_t *)ctx;
    if (!p || !alloc)
        return;
    if (p->conn)
        PQfinish(p->conn);
    if (p->connection_url)
        alloc->free(alloc->ctx, p->connection_url, strlen(p->connection_url) + 1);
    if (p->table_name)
        alloc->free(alloc->ctx, p->table_name, strlen(p->table_name) + 1);
    alloc->free(alloc->ctx, p, sizeof(pgv_ret_ctx_t));
}

static const hu_vector_store_vtable_t pgv_ret_vtable = {
    .insert = pgv_insert,
    .search = pgv_search,
    .remove = pgv_remove,
    .count = pgv_count,
    .deinit = pgv_deinit,
};

hu_vector_store_t hu_vector_store_pgvector_retrieval_create(hu_allocator_t *alloc,
                                                             const char *connection_url,
                                                             const char *table_name,
                                                             size_t dimensions) {
    hu_vector_store_t s = {.ctx = NULL, .vtable = NULL};
    if (!alloc || !connection_url || !connection_url[0])
        return s;
    const char *tbl = (table_name && table_name[0]) ? table_name : "memory_vectors";
    if (!pgv_safe_id(tbl))
        return s;
    size_t dim = dimensions > 0 ? dimensions : HU_EMBEDDING_DIM;
    pgv_ret_ctx_t *p = (pgv_ret_ctx_t *)alloc->alloc(alloc->ctx, sizeof(pgv_ret_ctx_t));
    if (!p)
        return s;
    memset(p, 0, sizeof(*p));
    p->alloc = alloc;
    p->connection_url = hu_strdup(alloc, connection_url);
    p->table_name = hu_strdup(alloc, tbl);
    p->dimensions = dim;
    if (!p->connection_url || !p->table_name) {
        pgv_deinit(p, alloc);
        return s;
    }
    p->conn = PQconnectdb(connection_url);
    if (!p->conn || PQstatus(p->conn) != CONNECTION_OK) {
        if (p->conn)
            PQfinish(p->conn);
        p->conn = NULL;
        pgv_deinit(p, alloc);
        return s;
    }
    if (!pgv_ensure_table(p->conn, p->table_name, dim)) {
        pgv_deinit(p, alloc);
        return s;
    }
    s.ctx = p;
    s.vtable = &pgv_ret_vtable;
    return s;
}

#else

hu_vector_store_t hu_vector_store_pgvector_retrieval_create(hu_allocator_t *alloc,
                                                              const char *connection_url,
                                                              const char *table_name,
                                                              size_t dimensions) {
    (void)connection_url;
    (void)table_name;
    (void)dimensions;
    hu_vector_store_t s = {.ctx = NULL, .vtable = NULL};
    (void)alloc;
    return s;
}

#endif /* HU_ENABLE_PGVECTOR && !HU_IS_TEST */
