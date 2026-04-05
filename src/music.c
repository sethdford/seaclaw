#include "human/music.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/platform.h"
#include <stdio.h>
#include <string.h>

#if !defined(HU_IS_TEST)
#include <time.h>
#include <unistd.h>
#endif

/* ── Helpers ─────────────────────────────────────────────────────────── */

static char *dup_json_str(hu_allocator_t *alloc, const char *s) {
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char *d = (char *)alloc->alloc(alloc->ctx, len + 1);
    if (d) {
        memcpy(d, s, len);
        d[len] = '\0';
    }
    return d;
}

static void free_str(hu_allocator_t *alloc, char **s) {
    if (*s) {
        alloc->free(alloc->ctx, *s, strlen(*s) + 1);
        *s = NULL;
    }
}

/* ── URL encoding ────────────────────────────────────────────────────── */

size_t hu_music_url_encode_query(const char *query, size_t query_len, char *out, size_t out_cap) {
    if (!query || query_len == 0 || !out || out_cap < 2)
        return 0;
    size_t w = 0;
    for (size_t i = 0; i < query_len && w + 1 < out_cap; i++) {
        unsigned char c = (unsigned char)query[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.') {
            out[w++] = (char)c;
        } else if (c == ' ') {
            out[w++] = '+';
        }
        /* other characters silently dropped — iTunes search is tolerant */
    }
    out[w] = '\0';
    return w;
}

/* ── JSON response parsing (pure, always compiled) ───────────────────── */

hu_error_t hu_music_parse_search_response(hu_allocator_t *alloc, const char *json, size_t json_len,
                                          hu_music_result_t *out) {
    if (!alloc || !json || json_len == 0 || !out)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));

    hu_json_value_t *root = NULL;
    hu_error_t err = hu_json_parse(alloc, json, json_len, &root);
    if (err != HU_OK)
        return err;

    hu_json_value_t *results = hu_json_object_get(root, "results");
    if (!results || results->type != HU_JSON_ARRAY || results->data.array.len == 0) {
        hu_json_free(alloc, root);
        return HU_ERR_NOT_FOUND;
    }

    hu_json_value_t *first = results->data.array.items[0];
    if (!first || first->type != HU_JSON_OBJECT) {
        hu_json_free(alloc, root);
        return HU_ERR_NOT_FOUND;
    }

    out->track_name = dup_json_str(alloc, hu_json_get_string(first, "trackName"));
    out->artist_name = dup_json_str(alloc, hu_json_get_string(first, "artistName"));
    out->album_name = dup_json_str(alloc, hu_json_get_string(first, "collectionName"));
    out->preview_url = dup_json_str(alloc, hu_json_get_string(first, "previewUrl"));
    out->track_view_url = dup_json_str(alloc, hu_json_get_string(first, "trackViewUrl"));
    out->artwork_url = dup_json_str(alloc, hu_json_get_string(first, "artworkUrl100"));
    out->genre = dup_json_str(alloc, hu_json_get_string(first, "primaryGenreName"));

    hu_json_free(alloc, root);

    if (!out->track_name && !out->artist_name) {
        hu_music_result_free(alloc, out);
        return HU_ERR_NOT_FOUND;
    }

    return HU_OK;
}

/* ── Share text builder (pure, always compiled) ──────────────────────── */

size_t hu_music_build_share_text(const hu_music_result_t *result, const char *casual_msg,
                                 size_t casual_msg_len, char *buf, size_t cap) {
    if (!result || !buf || cap < 32)
        return 0;

    const char *url = result->track_view_url;
    int n;

    if (casual_msg && casual_msg_len > 0 && url) {
        n = snprintf(buf, cap, "%.*s %s", (int)casual_msg_len, casual_msg, url);
    } else if (casual_msg && casual_msg_len > 0) {
        n = snprintf(buf, cap, "%.*s", (int)casual_msg_len, casual_msg);
    } else if (url) {
        n = snprintf(buf, cap, "%s - %s %s", result->artist_name ? result->artist_name : "Unknown",
                     result->track_name ? result->track_name : "Unknown", url);
    } else {
        n = snprintf(buf, cap, "%s - %s", result->artist_name ? result->artist_name : "Unknown",
                     result->track_name ? result->track_name : "Unknown");
    }

    return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
}

/* ── LLM suggestion parser (pure, always compiled) ───────────────────── */

bool hu_music_parse_suggestion(const char *suggestion, size_t suggestion_len, char *query_out,
                               size_t query_cap, char *msg_out, size_t msg_cap) {
    if (!suggestion || suggestion_len == 0 || !query_out || query_cap < 8 || !msg_out ||
        msg_cap < 8)
        return false;

    query_out[0] = '\0';
    msg_out[0] = '\0';

    /* Find the " | " separator between "ARTIST - TITLE" and casual message */
    const char *pipe = NULL;
    for (size_t i = 0; i + 2 < suggestion_len; i++) {
        if (suggestion[i] == ' ' && suggestion[i + 1] == '|' && suggestion[i + 2] == ' ') {
            pipe = suggestion + i;
            break;
        }
    }

    if (pipe) {
        size_t query_len = (size_t)(pipe - suggestion);
        if (query_len >= query_cap)
            query_len = query_cap - 1;
        memcpy(query_out, suggestion, query_len);
        query_out[query_len] = '\0';

        const char *msg_start = pipe + 3; /* skip " | " */
        size_t msg_len = suggestion_len - (size_t)(msg_start - suggestion);
        if (msg_len >= msg_cap)
            msg_len = msg_cap - 1;
        memcpy(msg_out, msg_start, msg_len);
        msg_out[msg_len] = '\0';
    } else {
        /* No pipe — treat entire string as the query, generate no casual message */
        size_t query_len = suggestion_len;
        if (query_len >= query_cap)
            query_len = query_cap - 1;
        memcpy(query_out, suggestion, query_len);
        query_out[query_len] = '\0';
    }

    /* Strip leading/trailing whitespace from query */
    size_t ql = strlen(query_out);
    while (ql > 0 && query_out[ql - 1] == ' ')
        query_out[--ql] = '\0';
    size_t start = 0;
    while (start < ql && query_out[start] == ' ')
        start++;
    if (start > 0) {
        memmove(query_out, query_out + start, ql - start + 1);
    }

    return query_out[0] != '\0';
}

/* ── Result cleanup ──────────────────────────────────────────────────── */

void hu_music_result_free(hu_allocator_t *alloc, hu_music_result_t *r) {
    if (!alloc || !r)
        return;
    free_str(alloc, &r->track_name);
    free_str(alloc, &r->artist_name);
    free_str(alloc, &r->album_name);
    free_str(alloc, &r->preview_url);
    free_str(alloc, &r->track_view_url);
    free_str(alloc, &r->artwork_url);
    free_str(alloc, &r->genre);
}

/* ── Network functions ───────────────────────────────────────────────── */

#if !defined(HU_IS_TEST) && defined(HU_HTTP_CURL)

#include "human/core/http.h"

hu_error_t hu_music_search(hu_allocator_t *alloc, const char *query, size_t query_len,
                           hu_music_result_t *out) {
    if (!alloc || !query || query_len == 0 || !out)
        return HU_ERR_INVALID_ARGUMENT;

    char encoded[256];
    size_t enc_len = hu_music_url_encode_query(query, query_len, encoded, sizeof(encoded));
    if (enc_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    char url[512];
    int n = snprintf(url, sizeof(url),
                     "https://itunes.apple.com/search?term=%s&media=music&entity=song&limit=1",
                     encoded);
    if (n < 0 || (size_t)n >= sizeof(url))
        return HU_ERR_INVALID_ARGUMENT;

    hu_http_response_t resp = {0};
    hu_error_t err = hu_http_get(alloc, url, NULL, &resp);
    if (err != HU_OK) {
        if (resp.owned && resp.body)
            hu_http_response_free(alloc, &resp);
        return err;
    }

    if (resp.status_code != 200 || !resp.body || resp.body_len == 0) {
        hu_http_response_free(alloc, &resp);
        return HU_ERR_IO;
    }

    err = hu_music_parse_search_response(alloc, resp.body, resp.body_len, out);
    hu_http_response_free(alloc, &resp);
    return err;
}

hu_error_t hu_music_download_preview(hu_allocator_t *alloc, const char *preview_url,
                                     char *path_out, size_t path_cap) {
    if (!alloc || !preview_url || !path_out || path_cap < 64)
        return HU_ERR_INVALID_ARGUMENT;

    hu_http_response_t resp = {0};
    hu_error_t err = hu_http_get(alloc, preview_url, NULL, &resp);
    if (err != HU_OK) {
        if (resp.owned && resp.body)
            hu_http_response_free(alloc, &resp);
        return err;
    }

    if (resp.status_code != 200 || !resp.body || resp.body_len == 0) {
        hu_http_response_free(alloc, &resp);
        return HU_ERR_IO;
    }

    char *tmp_dir = hu_platform_get_temp_dir(alloc);
    if (!tmp_dir) {
        hu_http_response_free(alloc, &resp);
        return HU_ERR_IO;
    }

    char tmpl[256];
    int tn = snprintf(tmpl, sizeof(tmpl), "%s/hu_music_XXXXXX.m4a", tmp_dir);
    alloc->free(alloc->ctx, tmp_dir, strlen(tmp_dir) + 1);
    if (tn < 0 || (size_t)tn >= sizeof(tmpl)) {
        hu_http_response_free(alloc, &resp);
        return HU_ERR_IO;
    }

    int fd = mkstemps(tmpl, 4); /* 4 = strlen(".m4a") */
    if (fd < 0) {
        hu_http_response_free(alloc, &resp);
        return HU_ERR_IO;
    }

    FILE *f = fdopen(fd, "wb");
    if (!f) {
        close(fd);
        unlink(tmpl);
        hu_http_response_free(alloc, &resp);
        return HU_ERR_IO;
    }

    size_t written = fwrite(resp.body, 1, resp.body_len, f);
    fclose(f);
    hu_http_response_free(alloc, &resp);

    if (written != resp.body_len) {
        unlink(tmpl);
        return HU_ERR_IO;
    }

    size_t path_len = strlen(tmpl);
    if (path_len >= path_cap) {
        unlink(tmpl);
        return HU_ERR_INVALID_ARGUMENT;
    }

    memcpy(path_out, tmpl, path_len + 1);
    return HU_OK;
}

#else /* HU_IS_TEST || !HU_HTTP_CURL */

hu_error_t hu_music_search(hu_allocator_t *alloc, const char *query, size_t query_len,
                           hu_music_result_t *out) {
    (void)alloc;
    (void)query;
    (void)query_len;
    (void)out;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_music_download_preview(hu_allocator_t *alloc, const char *preview_url,
                                     char *path_out, size_t path_cap) {
    (void)alloc;
    (void)preview_url;
    (void)path_out;
    (void)path_cap;
    return HU_ERR_NOT_SUPPORTED;
}

#endif
