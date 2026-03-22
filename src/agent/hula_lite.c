#include "human/agent/hula_lite.h"
#include "human/core/string.h"
#include <stdlib.h>
#include <string.h>

#define HU_HULA_LITE_MAX_LINES 128
#define HU_HULA_LITE_LINE 384

typedef struct {
    int ind;
    char text[HU_HULA_LITE_LINE];
} hula_lite_line_t;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    hu_allocator_t *alloc;
} hula_bb_t;

static void bb_free(hula_bb_t *b) {
    if (b->data && b->alloc)
        b->alloc->free(b->alloc->ctx, b->data, b->cap);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static hu_error_t bb_reserve(hula_bb_t *b, size_t need) {
    if (b->len + need + 1 <= b->cap)
        return HU_OK;
    size_t nc = b->cap ? b->cap * 2u : 512u;
    while (nc < b->len + need + 1)
        nc *= 2u;
    char *np = (char *)b->alloc->alloc(b->alloc->ctx, nc);
    if (!np)
        return HU_ERR_OUT_OF_MEMORY;
    if (b->data && b->len)
        memcpy(np, b->data, b->len);
    if (b->data)
        b->alloc->free(b->alloc->ctx, b->data, b->cap);
    b->data = np;
    b->cap = nc;
    return HU_OK;
}

static hu_error_t bb_append(hula_bb_t *b, const char *s, size_t n) {
    hu_error_t e = bb_reserve(b, n);
    if (e != HU_OK)
        return e;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return HU_OK;
}

static hu_error_t bb_putc(hula_bb_t *b, char c) {
    return bb_append(b, &c, 1);
}

static hu_error_t bb_append_esc(hula_bb_t *b, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') {
            if (bb_putc(b, '\\') != HU_OK)
                return HU_ERR_OUT_OF_MEMORY;
        }
        if (bb_putc(b, (char)c) != HU_OK)
            return HU_ERR_OUT_OF_MEMORY;
    }
    return HU_OK;
}

static int line_indent(const char *line, size_t len, size_t *skip) {
    size_t i = 0;
    while (i < len && line[i] == ' ')
        i++;
    if ((i % 2u) != 0)
        return -1;
    *skip = i;
    return (int)(i / 2u);
}

static size_t split_words(const char *s, size_t len, char *w0, char *w1, char *w2, size_t wcap) {
    size_t n = 0;
    const char *p = s;
    size_t rem = len;
    char *outs[3] = {w0, w1, w2};
    for (size_t wi = 0; wi < 3; wi++) {
        while (rem > 0 && (*p == ' ' || *p == '\t')) {
            p++;
            rem--;
        }
        if (rem == 0) {
            outs[wi][0] = '\0';
            continue;
        }
        size_t j = 0;
        while (rem > 0 && *p != ' ' && *p != '\t' && j + 1 < wcap) {
            outs[wi][j++] = *p++;
            rem--;
        }
        outs[wi][j] = '\0';
        n++;
    }
    (void)n;
    return n;
}

static hu_error_t collect_lines(const char *src, size_t src_len, hula_lite_line_t *out,
                                size_t *out_count) {
    *out_count = 0;
    size_t i = 0;
    while (i < src_len) {
        size_t start = i;
        while (i < src_len && src[i] != '\n' && src[i] != '\r')
            i++;
        size_t line_len = i - start;
        while (i < src_len && (src[i] == '\n' || src[i] == '\r'))
            i++;

        while (line_len > 0 && (unsigned char)src[start + line_len - 1] <= 32)
            line_len--;
        if (line_len == 0)
            continue;

        bool any_non_ws = false;
        for (size_t z = 0; z < line_len; z++) {
            if (!((unsigned char)src[start + z] <= 32)) {
                any_non_ws = true;
                break;
            }
        }
        if (!any_non_ws)
            continue;

        size_t skip = 0;
        int ind = line_indent(src + start, line_len, &skip);
        if (ind < 0)
            return HU_ERR_PARSE;
        if (skip >= line_len)
            continue;
        if (src[start + skip] == '#')
            continue;
        if (*out_count >= HU_HULA_LITE_MAX_LINES)
            return HU_ERR_INVALID_ARGUMENT;

        size_t tlen = line_len - skip;
        if (tlen >= HU_HULA_LITE_LINE)
            return HU_ERR_INVALID_ARGUMENT;
        memcpy(out[*out_count].text, src + start + skip, tlen);
        out[*out_count].text[tlen] = '\0';
        out[*out_count].ind = ind;
        (*out_count)++;
    }
    return HU_OK;
}

static hu_error_t emit_call(hula_bb_t *bb, const hula_lite_line_t *lines, size_t nlines, size_t *idx,
                            int my_ind, const char *id, const char *tool) {
    const char *call_head = "{\"op\":\"call\",\"id\":\"";
    if (bb_append(bb, call_head, strlen(call_head)) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;
    if (bb_append_esc(bb, id, strlen(id)) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;
    const char *tool_mid = "\",\"tool\":\"";
    if (bb_append(bb, tool_mid, strlen(tool_mid)) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;
    if (bb_append_esc(bb, tool, strlen(tool)) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;
    const char *args_open = "\",\"args\":{";
    if (bb_append(bb, args_open, strlen(args_open)) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;

    (*idx)++;
    bool first = true;
    while (*idx < nlines && lines[*idx].ind == my_ind + 1) {
        char k[64], v0[64], v1[64];
        split_words(lines[*idx].text, strlen(lines[*idx].text), k, v0, v1, sizeof(k));
        if (!k[0]) {
            (*idx)++;
            continue;
        }
        const char *ln = lines[*idx].text;
        size_t rl = strlen(ln);
        size_t kend = 0;
        while (kend < rl && ln[kend] != ' ' && ln[kend] != '\t')
            kend++;
        while (kend < rl && (ln[kend] == ' ' || ln[kend] == '\t'))
            kend++;
        const char *val = ln + kend;
        size_t vl = rl - kend;

        if (!first && bb_append(bb, ",", strlen(",")) != HU_OK)
            return HU_ERR_OUT_OF_MEMORY;
        first = false;
        if (bb_append(bb, "\"", strlen("\"")) != HU_OK)
            return HU_ERR_OUT_OF_MEMORY;
        if (bb_append_esc(bb, k, strlen(k)) != HU_OK)
            return HU_ERR_OUT_OF_MEMORY;
        if (bb_append(bb, "\":\"", strlen("\":\"")) != HU_OK)
            return HU_ERR_OUT_OF_MEMORY;
        if (bb_append_esc(bb, val, vl) != HU_OK)
            return HU_ERR_OUT_OF_MEMORY;
        if (bb_append(bb, "\"", strlen("\"")) != HU_OK)
            return HU_ERR_OUT_OF_MEMORY;
        (*idx)++;
    }
    if (bb_append(bb, "}}", strlen("}}")) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;
    return HU_OK;
}

static hu_error_t emit_seq_par(hula_bb_t *bb, const hula_lite_line_t *lines, size_t nlines, size_t *idx,
                               int my_ind, const char *op, const char *id);

static hu_error_t emit_node(hula_bb_t *bb, const hula_lite_line_t *lines, size_t nlines, size_t *idx,
                            int expect_ind) {
    if (*idx >= nlines || lines[*idx].ind != expect_ind)
        return HU_ERR_PARSE;
    char w0[64], w1[64], w2[64];
    split_words(lines[*idx].text, strlen(lines[*idx].text), w0, w1, w2, sizeof(w0));
    if (!w0[0])
        return HU_ERR_PARSE;
    int my_ind = lines[*idx].ind;

    if (strcmp(w0, "call") == 0) {
        if (!w1[0] || !w2[0])
            return HU_ERR_PARSE;
        return emit_call(bb, lines, nlines, idx, my_ind, w1, w2);
    }
    if (strcmp(w0, "seq") == 0 || strcmp(w0, "par") == 0) {
        if (!w1[0])
            return HU_ERR_PARSE;
        return emit_seq_par(bb, lines, nlines, idx, my_ind, w0, w1);
    }
    return HU_ERR_PARSE;
}

static hu_error_t emit_seq_par(hula_bb_t *bb, const hula_lite_line_t *lines, size_t nlines, size_t *idx,
                               int my_ind, const char *op, const char *id) {
    const char *op_head = "{\"op\":\"";
    if (bb_append(bb, op_head, strlen(op_head)) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;
    if (bb_append(bb, op, strlen(op)) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;
    const char *id_mid = "\",\"id\":\"";
    if (bb_append(bb, id_mid, strlen(id_mid)) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;
    if (bb_append_esc(bb, id, strlen(id)) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;
    const char *kids = "\",\"children\":[";
    if (bb_append(bb, kids, strlen(kids)) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;
    (*idx)++;
    bool first = true;
    while (*idx < nlines && lines[*idx].ind == my_ind + 1) {
        if (!first && bb_append(bb, ",", strlen(",")) != HU_OK)
            return HU_ERR_OUT_OF_MEMORY;
        first = false;
        hu_error_t e = emit_node(bb, lines, nlines, idx, my_ind + 1);
        if (e != HU_OK)
            return e;
    }
    if (bb_append(bb, "]}", strlen("]}")) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;
    return HU_OK;
}

hu_error_t hu_hula_lite_to_json(hu_allocator_t *alloc, const char *src, size_t src_len, char **out,
                                size_t *out_len) {
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;
    if (!src || src_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    hula_lite_line_t lines[HU_HULA_LITE_MAX_LINES];
    size_t nlines = 0;
    hu_error_t err = collect_lines(src, src_len, lines, &nlines);
    if (err != HU_OK)
        return err;
    if (nlines < 2)
        return HU_ERR_PARSE;

    char w0[64], w1[64], w2[64];
    split_words(lines[0].text, strlen(lines[0].text), w0, w1, w2, sizeof(w0));
    if (strcmp(w0, "program") != 0 || !w1[0])
        return HU_ERR_PARSE;

    hula_bb_t bb = {.alloc = alloc};
    const char *prog_name = "{\"name\":\"";
    err = bb_append(&bb, prog_name, strlen(prog_name));
    if (err == HU_OK)
        err = bb_append_esc(&bb, w1, strlen(w1));
    const char *prog_tail = "\",\"version\":1,\"root\":";
    if (err == HU_OK)
        err = bb_append(&bb, prog_tail, strlen(prog_tail));
    if (err != HU_OK) {
        bb_free(&bb);
        return err;
    }

    size_t idx = 1;
    err = emit_node(&bb, lines, nlines, &idx, lines[1].ind);
    if (err != HU_OK || idx != nlines) {
        bb_free(&bb);
        return err != HU_OK ? err : HU_ERR_PARSE;
    }
    err = bb_putc(&bb, '}');
    if (err != HU_OK) {
        bb_free(&bb);
        return err;
    }

    *out = bb.data;
    *out_len = bb.len;
    bb.data = NULL;
    bb_free(&bb);
    return HU_OK;
}
