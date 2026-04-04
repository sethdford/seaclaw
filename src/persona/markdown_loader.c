#include "human/persona/markdown_loader.h"

#include "human/core/slice.h"
#include "human/core/string.h"
#include "human/persona.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <dirent.h>
#endif

enum { hu_md_max_file_bytes = 256 * 1024 };
enum { hu_md_max_discover_files = 256 };
enum { hu_md_peek_bytes = 16384 };

#if defined(HU_IS_TEST)
hu_error_t hu_persona_load_markdown_buffer(hu_allocator_t *alloc, const char *content,
                                           size_t content_len, hu_persona_t *out);
#endif

static size_t skip_bom(const char *s, size_t len) {
    if (len >= 3 && (unsigned char)s[0] == 0xef && (unsigned char)s[1] == 0xbb &&
        (unsigned char)s[2] == 0xbf) {
        return 3;
    }
    return 0;
}

static size_t count_leading_spaces(const char *line, size_t len) {
    size_t i = 0;
    for (; i < len && line[i] == ' '; i++) {
    }
    return i;
}

static void trim_ws_inplace(const char **ptr, size_t *len) {
    const char *p = *ptr;
    size_t n = *len;
    while (n > 0 && isspace((unsigned char)p[0])) {
        p++;
        n--;
    }
    while (n > 0 && isspace((unsigned char)p[n - 1])) {
        n--;
    }
    *ptr = p;
    *len = n;
}

static bool line_is_fence(const char *line, size_t len) {
    trim_ws_inplace(&line, &len);
    return len == 3 && line[0] == '-' && line[1] == '-' && line[2] == '-';
}

/* If no opening ---, *body = full slice. If opening --- without closing ---, returns HU_ERR_PARSE. */
static hu_error_t split_frontmatter(const char *data, size_t len, const char **fm_start,
                                    size_t *fm_len, const char **body_start, size_t *body_len) {
    size_t off = skip_bom(data, len);
    const char *p = data + off;
    size_t rem = len - off;

    /* Skip blank lines before first fence */
    while (rem > 0) {
        const char *nl = (const char *)memchr(p, '\n', rem);
        size_t line_len = nl ? (size_t)(nl - p) : rem;
        if (line_len && p[line_len - 1] == '\r') {
            line_len--;
        }
        const char *line = p;
        if (!line_is_fence(line, line_len)) {
            if (line_len == 0 && nl) {
                p = nl + 1;
                rem = len - (size_t)(p - data);
                continue;
            }
            /* No YAML fence: entire content is body */
            *fm_start = p;
            *fm_len = 0;
            *body_start = data + off;
            *body_len = len - off;
            return HU_OK;
        }
        /* Opening fence */
        p = nl ? nl + 1 : p + rem;
        rem = len - (size_t)(p - data);
        break;
    }

    const char *fm_begin = p;
    const char *close = NULL;
    while (rem > 0) {
        const char *nl = (const char *)memchr(p, '\n', rem);
        size_t line_len = nl ? (size_t)(nl - p) : rem;
        if (line_len && p[line_len - 1] == '\r') {
            line_len--;
        }
        if (line_is_fence(p, line_len)) {
            close = p;
            break;
        }
        p = nl ? nl + 1 : p + rem;
        rem = len - (size_t)(p - data);
    }
    if (!close) {
        return HU_ERR_PARSE;
    }

    *fm_start = fm_begin;
    *fm_len = (size_t)(close - fm_begin);
    const char *body = close;
    /* skip closing fence line */
    const char *nl_after = (const char *)memchr(body, '\n', len - (size_t)(body - data));
    if (nl_after) {
        body = nl_after + 1;
    } else {
        body = data + len;
    }
    *body_start = body;
    if (body <= data + len) {
        *body_len = (size_t)((data + len) - body);
    } else {
        *body_len = 0;
    }
    return HU_OK;
}

static hu_error_t str_array_push(hu_allocator_t *a, char ***arr, size_t *count, size_t *cap,
                                 const char *val, size_t val_len) {
    if (*count >= *cap) {
        size_t nc = *cap ? *cap * 2 : 8U;
        size_t ob = *cap * sizeof(char *);
        size_t nb = nc * sizeof(char *);
        char **nn;
        if (*arr && a->realloc) {
            nn = (char **)a->realloc(a->ctx, *arr, ob, nb);
        } else {
            nn = (char **)a->alloc(a->ctx, nb);
            if (nn && *arr && *count > 0) {
                memcpy(nn, *arr, *count * sizeof(char *));
            }
            if (nn && *arr && *cap > 0 && !a->realloc) {
                a->free(a->ctx, *arr, ob);
            }
        }
        if (!nn) {
            return HU_ERR_OUT_OF_MEMORY;
        }
        *arr = nn;
        *cap = nc;
    }
    char *copy = hu_strndup(a, val, val_len);
    if (!copy) {
        return HU_ERR_OUT_OF_MEMORY;
    }
    (*arr)[(*count)++] = copy;
    return HU_OK;
}

static void free_discard_strings(hu_allocator_t *a, char **items, size_t n, size_t cap_slots) {
    if (!a || !items) {
        return;
    }
    for (size_t i = 0; i < n; i++) {
        if (items[i]) {
            size_t z = strlen(items[i]) + 1U;
            a->free(a->ctx, items[i], z);
        }
    }
    if (cap_slots > 0U) {
        a->free(a->ctx, items, cap_slots * sizeof(char *));
    }
}

static bool parse_top_key_value(const char *line, size_t line_len, char *key_buf, size_t key_cap,
                                const char **val_out, size_t *val_len_out) {
    size_t sp = count_leading_spaces(line, line_len);
    if (sp != 0U) {
        return false;
    }
    const char *colon = (const char *)memchr(line, ':', line_len);
    if (!colon) {
        return false;
    }
    size_t key_len = (size_t)(colon - line);
    if (key_len == 0U || key_len + 1U > key_cap) {
        return false;
    }
    for (size_t i = 0; i < key_len; i++) {
        unsigned char c = (unsigned char)line[i];
        if (!(isalnum(c) != 0 || c == (unsigned char)'_')) {
            return false;
        }
    }
    memcpy(key_buf, line, key_len);
    key_buf[key_len] = '\0';
    const char *v = colon + 1;
    size_t vl = line_len - (size_t)(v - line);
    trim_ws_inplace(&v, &vl);
    *val_out = v;
    *val_len_out = vl;
    return true;
}

static bool parse_indented_key_value(const char *line, size_t line_len, size_t min_indent,
                                     char *key_buf, size_t key_cap, const char **val_out,
                                     size_t *val_len_out) {
    size_t sp = count_leading_spaces(line, line_len);
    if (sp < min_indent) {
        return false;
    }
    const char *sub = line + sp;
    size_t sub_len = line_len - sp;
    const char *colon = (const char *)memchr(sub, ':', sub_len);
    if (!colon) {
        return false;
    }
    size_t key_len = (size_t)(colon - sub);
    if (key_len == 0U || key_len + 1U > key_cap) {
        return false;
    }
    memcpy(key_buf, sub, key_len);
    key_buf[key_len] = '\0';
    const char *v = colon + 1;
    size_t vl = sub_len - (size_t)(v - sub);
    trim_ws_inplace(&v, &vl);
    *val_out = v;
    *val_len_out = vl;
    return true;
}

static hu_error_t shrink_string_pointer_array(hu_allocator_t *a, char ***arr, size_t *cap,
                                              size_t count) {
    if (count == 0U) {
        if (*arr != NULL && *cap > 0U) {
            a->free(a->ctx, *arr, *cap * sizeof(char *));
            *arr = NULL;
            *cap = 0U;
        }
        return HU_OK;
    }
    if (*cap == count) {
        return HU_OK;
    }
    size_t ob = *cap * sizeof(char *);
    size_t nb = count * sizeof(char *);
    char **nn;
    if (*arr != NULL && a->realloc) {
        nn = (char **)a->realloc(a->ctx, *arr, ob, nb);
    } else {
        nn = (char **)a->alloc(a->ctx, nb);
        if (!nn) {
            return HU_ERR_OUT_OF_MEMORY;
        }
        memcpy(nn, *arr, count * sizeof(char *));
        if (*arr != NULL && *cap > 0U) {
            a->free(a->ctx, *arr, ob);
        }
    }
    if (!nn) {
        return HU_ERR_OUT_OF_MEMORY;
    }
    *arr = nn;
    *cap = count;
    return HU_OK;
}

static hu_error_t shrink_overlay_array(hu_allocator_t *a, hu_persona_overlay_t **arr, size_t *cap,
                                       size_t count) {
    if (count == 0U) {
        if (*arr != NULL && *cap > 0U) {
            a->free(a->ctx, *arr, *cap * sizeof(hu_persona_overlay_t));
            *arr = NULL;
            *cap = 0U;
        }
        return HU_OK;
    }
    if (*cap == count) {
        return HU_OK;
    }
    size_t ob = *cap * sizeof(hu_persona_overlay_t);
    size_t nb = count * sizeof(hu_persona_overlay_t);
    hu_persona_overlay_t *nn;
    if (*arr != NULL && a->realloc) {
        nn = (hu_persona_overlay_t *)a->realloc(a->ctx, *arr, ob, nb);
    } else {
        nn = (hu_persona_overlay_t *)a->alloc(a->ctx, nb);
        if (!nn) {
            return HU_ERR_OUT_OF_MEMORY;
        }
        memcpy(nn, *arr, count * sizeof(hu_persona_overlay_t));
        if (*arr != NULL && *cap > 0U) {
            a->free(a->ctx, *arr, ob);
        }
    }
    if (!nn) {
        return HU_ERR_OUT_OF_MEMORY;
    }
    *arr = nn;
    *cap = count;
    return HU_OK;
}

static hu_error_t overlay_flush(hu_allocator_t *a, hu_persona_overlay_t *cur, bool has_channel,
                                hu_persona_overlay_t **ovs, size_t *ov_count, size_t *ov_cap) {
    if (!has_channel || !cur->channel) {
        return HU_OK;
    }
    if (*ov_count >= *ov_cap) {
        size_t nc = *ov_cap ? *ov_cap * 2U : 4U;
        size_t ob = *ov_cap * sizeof(hu_persona_overlay_t);
        size_t nb = nc * sizeof(hu_persona_overlay_t);
        hu_persona_overlay_t *nn;
        if (*ovs && a->realloc) {
            nn = (hu_persona_overlay_t *)a->realloc(a->ctx, *ovs, ob, nb);
        } else {
            nn = (hu_persona_overlay_t *)a->alloc(a->ctx, nb);
            if (nn && *ovs && *ov_count > 0U) {
                memcpy(nn, *ovs, *ov_count * sizeof(hu_persona_overlay_t));
            }
            if (nn && *ovs && *ov_cap > 0U && !a->realloc) {
                a->free(a->ctx, *ovs, ob);
            }
        }
        if (!nn) {
            return HU_ERR_OUT_OF_MEMORY;
        }
        *ovs = nn;
        *ov_cap = nc;
    }
    (*ovs)[*ov_count] = *cur;
    memset(cur, 0, sizeof(*cur));
    (*ov_count)++;
    return HU_OK;
}

typedef enum {
    MD_MODE_TOP,
    MD_MODE_TRAITS,
    MD_MODE_TOOLS,
    MD_MODE_CHANNELS,
    MD_MODE_OVERLAYS
} md_parse_mode_t;

static hu_error_t parse_frontmatter(hu_allocator_t *alloc, const char *fm, size_t fm_len,
                                    hu_persona_t *out, char **identity_fm_owned) {
    *identity_fm_owned = NULL;

    md_parse_mode_t mode = MD_MODE_TOP;
    hu_persona_overlay_t cur_ov;
    memset(&cur_ov, 0, sizeof(cur_ov));
    bool have_ov_channel = false;
    hu_persona_overlay_t *overlays = NULL;
    size_t ov_count = 0;
    size_t ov_cap = 0;

    char **traits = NULL;
    size_t traits_n = 0;
    size_t traits_cap = 0;

    char **discard = NULL;
    size_t discard_n = 0;
    size_t discard_cap = 0;

    const char *p = fm;
    const char *end = fm + fm_len;

    while (p < end) {
        const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
        const char *line = p;
        size_t line_len = nl ? (size_t)(nl - p) : (size_t)(end - p);
        if (line_len && line[line_len - 1] == '\r') {
            line_len--;
        }
        p = nl ? nl + 1 : end;

        if (mode == MD_MODE_TRAITS || mode == MD_MODE_TOOLS || mode == MD_MODE_CHANNELS) {
            size_t ind = count_leading_spaces(line, line_len);
            if (ind == 0U && line_len > 0U) {
                /* Array block ended */
                if (mode == MD_MODE_TOOLS || mode == MD_MODE_CHANNELS) {
                    free_discard_strings(alloc, discard, discard_n, discard_cap);
                    discard = NULL;
                    discard_n = 0;
                    discard_cap = 0;
                }
                mode = MD_MODE_TOP;
                /* fall through to re-parse this line as top */
            } else {
                const char *rest = line + ind;
                size_t rest_len = line_len - ind;
                trim_ws_inplace(&rest, &rest_len);
                if (rest_len >= 2U && rest[0] == '-' && isspace((unsigned char)rest[1])) {
                    const char *item = rest + 2;
                    size_t item_len = rest_len - 2U;
                    trim_ws_inplace(&item, &item_len);
                    hu_error_t e;
                    if (mode == MD_MODE_TRAITS) {
                        e = str_array_push(alloc, &traits, &traits_n, &traits_cap, item, item_len);
                    } else {
                        e = str_array_push(alloc, &discard, &discard_n, &discard_cap, item,
                                           item_len);
                    }
                    if (e != HU_OK) {
                        free_discard_strings(alloc, discard, discard_n, discard_cap);
                        for (size_t i = 0; i < ov_count; i++) {
                            hu_persona_overlay_t *o = &overlays[i];
                            if (o->channel) {
                                alloc->free(alloc->ctx, o->channel, strlen(o->channel) + 1U);
                            }
                            if (o->formality) {
                                alloc->free(alloc->ctx, o->formality, strlen(o->formality) + 1U);
                            }
                            if (o->avg_length) {
                                alloc->free(alloc->ctx, o->avg_length, strlen(o->avg_length) + 1U);
                            }
                        }
                        if (overlays) {
                            alloc->free(alloc->ctx, overlays, ov_cap * sizeof(hu_persona_overlay_t));
                        }
                        for (size_t i = 0; i < traits_n; i++) {
                            alloc->free(alloc->ctx, traits[i], strlen(traits[i]) + 1U);
                        }
                        if (traits != NULL && traits_cap > 0U) {
                            alloc->free(alloc->ctx, traits, traits_cap * sizeof(char *));
                        }
                        if (*identity_fm_owned) {
                            alloc->free(alloc->ctx, *identity_fm_owned,
                                        strlen(*identity_fm_owned) + 1U);
                            *identity_fm_owned = NULL;
                        }
                        if (cur_ov.channel) {
                            alloc->free(alloc->ctx, cur_ov.channel, strlen(cur_ov.channel) + 1U);
                        }
                        if (cur_ov.formality) {
                            alloc->free(alloc->ctx, cur_ov.formality, strlen(cur_ov.formality) + 1U);
                        }
                        if (cur_ov.avg_length) {
                            alloc->free(alloc->ctx, cur_ov.avg_length, strlen(cur_ov.avg_length) + 1U);
                        }
                        return e;
                    }
                }
                continue;
            }
        }

        if (mode == MD_MODE_OVERLAYS) {
            size_t ind = count_leading_spaces(line, line_len);
            if (ind == 0U && line_len > 0U) {
                hu_error_t fe = overlay_flush(alloc, &cur_ov, have_ov_channel, &overlays,
                                              &ov_count, &ov_cap);
                have_ov_channel = false;
                if (fe != HU_OK) {
                    /* cleanup same as OOM above — use goto in refactor; duplicate minimal */
                    for (size_t i = 0; i < ov_count; i++) {
                        hu_persona_overlay_t *o = &overlays[i];
                        if (o->channel) {
                            alloc->free(alloc->ctx, o->channel, strlen(o->channel) + 1U);
                        }
                        if (o->formality) {
                            alloc->free(alloc->ctx, o->formality, strlen(o->formality) + 1U);
                        }
                        if (o->avg_length) {
                            alloc->free(alloc->ctx, o->avg_length, strlen(o->avg_length) + 1U);
                        }
                    }
                    if (overlays) {
                        alloc->free(alloc->ctx, overlays, ov_cap * sizeof(hu_persona_overlay_t));
                    }
                    for (size_t i = 0; i < traits_n; i++) {
                        alloc->free(alloc->ctx, traits[i], strlen(traits[i]) + 1U);
                    }
                    if (traits != NULL && traits_cap > 0U) {
                        alloc->free(alloc->ctx, traits, traits_cap * sizeof(char *));
                    }
                    if (*identity_fm_owned) {
                        alloc->free(alloc->ctx, *identity_fm_owned,
                                    strlen(*identity_fm_owned) + 1U);
                        *identity_fm_owned = NULL;
                    }
                    return fe;
                }
                mode = MD_MODE_TOP;
                /* fall through */
            } else if (ind == 2U) {
                const char *rest = line + 2U;
                size_t rest_len = line_len - 2U;
                const char *colon = (const char *)memchr(rest, ':', rest_len);
                if (colon) {
                    hu_error_t fe = overlay_flush(alloc, &cur_ov, have_ov_channel, &overlays,
                                                  &ov_count, &ov_cap);
                    if (fe != HU_OK) {
                        return fe;
                    }
                    have_ov_channel = true;
                    memset(&cur_ov, 0, sizeof(cur_ov));
                    size_t ch_len = (size_t)(colon - rest);
                    cur_ov.channel = hu_strndup(alloc, rest, ch_len);
                    if (!cur_ov.channel) {
                        return HU_ERR_OUT_OF_MEMORY;
                    }
                }
                continue;
            } else if (ind >= 4U) {
                char subk[48];
                const char *vv;
                size_t vl;
                if (parse_indented_key_value(line, line_len, 4U, subk, sizeof(subk), &vv, &vl)) {
                    if (strcmp(subk, "formality") == 0 && vl > 0U) {
                        if (cur_ov.formality) {
                            alloc->free(alloc->ctx, cur_ov.formality,
                                        strlen(cur_ov.formality) + 1U);
                        }
                        cur_ov.formality = hu_strndup(alloc, vv, vl);
                        if (!cur_ov.formality) {
                            return HU_ERR_OUT_OF_MEMORY;
                        }
                    } else if (strcmp(subk, "avg_length") == 0 && vl > 0U) {
                        if (cur_ov.avg_length) {
                            alloc->free(alloc->ctx, cur_ov.avg_length,
                                        strlen(cur_ov.avg_length) + 1U);
                        }
                        cur_ov.avg_length = hu_strndup(alloc, vv, vl);
                        if (!cur_ov.avg_length) {
                            return HU_ERR_OUT_OF_MEMORY;
                        }
                    }
                }
                continue;
            } else {
                continue;
            }
        }

        if (line_len == 0U) {
            continue;
        }

        char key[64];
        const char *val;
        size_t vlen;
        if (!parse_top_key_value(line, line_len, key, sizeof(key), &val, &vlen)) {
            continue;
        }

        if (strcmp(key, "traits") == 0) {
            mode = MD_MODE_TRAITS;
        } else if (strcmp(key, "tools") == 0) {
            mode = MD_MODE_TOOLS;
        } else if (strcmp(key, "channels") == 0) {
            mode = MD_MODE_CHANNELS;
        } else if (strcmp(key, "overlays") == 0) {
            mode = MD_MODE_OVERLAYS;
        } else if (strcmp(key, "name") == 0 && vlen > 0U) {
            if (out->name) {
                alloc->free(alloc->ctx, out->name, out->name_len + 1U);
            }
            out->name = hu_strndup(alloc, val, vlen);
            if (!out->name) {
                return HU_ERR_OUT_OF_MEMORY;
            }
            out->name_len = strlen(out->name);
        } else if (strcmp(key, "identity") == 0 && vlen > 0U) {
            if (*identity_fm_owned) {
                alloc->free(alloc->ctx, *identity_fm_owned,
                            strlen(*identity_fm_owned) + 1U);
            }
            *identity_fm_owned = hu_strndup(alloc, val, vlen);
            if (!*identity_fm_owned) {
                return HU_ERR_OUT_OF_MEMORY;
            }
        } else if (strcmp(key, "model") == 0 && vlen > 0U) {
            size_t cpy = vlen < sizeof(out->voice.model) - 1U ? vlen : sizeof(out->voice.model) - 1U;
            memcpy(out->voice.model, val, cpy);
            out->voice.model[cpy] = '\0';
        } else if (strcmp(key, "temperature") == 0) {
            /* Parsed for forward compatibility; hu_persona_t has no scalar temperature field. */
        }
    }

    if (mode == MD_MODE_TOOLS || mode == MD_MODE_CHANNELS) {
        free_discard_strings(alloc, discard, discard_n, discard_cap);
        discard = NULL;
    }

    if (mode == MD_MODE_OVERLAYS) {
        hu_error_t fe = overlay_flush(alloc, &cur_ov, have_ov_channel, &overlays, &ov_count,
                                        &ov_cap);
        if (fe != HU_OK) {
            return fe;
        }
    }

    hu_error_t sh = shrink_string_pointer_array(alloc, &traits, &traits_cap, traits_n);
    if (sh != HU_OK) {
        for (size_t i = 0; i < traits_n; i++) {
            alloc->free(alloc->ctx, traits[i], strlen(traits[i]) + 1U);
        }
        if (traits != NULL && traits_cap > 0U) {
            alloc->free(alloc->ctx, traits, traits_cap * sizeof(char *));
        }
        for (size_t i = 0; i < ov_count; i++) {
            hu_persona_overlay_t *o = &overlays[i];
            if (o->channel) {
                alloc->free(alloc->ctx, o->channel, strlen(o->channel) + 1U);
            }
            if (o->formality) {
                alloc->free(alloc->ctx, o->formality, strlen(o->formality) + 1U);
            }
            if (o->avg_length) {
                alloc->free(alloc->ctx, o->avg_length, strlen(o->avg_length) + 1U);
            }
        }
        if (overlays != NULL && ov_cap > 0U) {
            alloc->free(alloc->ctx, overlays, ov_cap * sizeof(hu_persona_overlay_t));
        }
        if (*identity_fm_owned) {
            alloc->free(alloc->ctx, *identity_fm_owned, strlen(*identity_fm_owned) + 1U);
            *identity_fm_owned = NULL;
        }
        return sh;
    }
    sh = shrink_overlay_array(alloc, &overlays, &ov_cap, ov_count);
    if (sh != HU_OK) {
        for (size_t i = 0; i < traits_n; i++) {
            alloc->free(alloc->ctx, traits[i], strlen(traits[i]) + 1U);
        }
        if (traits != NULL && traits_cap > 0U) {
            alloc->free(alloc->ctx, traits, traits_cap * sizeof(char *));
        }
        for (size_t i = 0; i < ov_count; i++) {
            hu_persona_overlay_t *o = &overlays[i];
            if (o->channel) {
                alloc->free(alloc->ctx, o->channel, strlen(o->channel) + 1U);
            }
            if (o->formality) {
                alloc->free(alloc->ctx, o->formality, strlen(o->formality) + 1U);
            }
            if (o->avg_length) {
                alloc->free(alloc->ctx, o->avg_length, strlen(o->avg_length) + 1U);
            }
        }
        if (overlays != NULL && ov_cap > 0U) {
            alloc->free(alloc->ctx, overlays, ov_cap * sizeof(hu_persona_overlay_t));
        }
        if (*identity_fm_owned) {
            alloc->free(alloc->ctx, *identity_fm_owned, strlen(*identity_fm_owned) + 1U);
            *identity_fm_owned = NULL;
        }
        return sh;
    }

    out->traits = traits;
    out->traits_count = traits_n;
    out->overlays = overlays;
    out->overlays_count = ov_count;
    return HU_OK;
}

static char *concat_identity_body(hu_allocator_t *alloc, char *identity_fm, const char *body,
                                  size_t body_len) {
    trim_ws_inplace(&body, &body_len);
    if ((!identity_fm || identity_fm[0] == '\0') && body_len == 0U) {
        if (identity_fm) {
            alloc->free(alloc->ctx, identity_fm, strlen(identity_fm) + 1U);
        }
        return NULL;
    }
    if (!identity_fm || identity_fm[0] == '\0') {
        if (identity_fm) {
            alloc->free(alloc->ctx, identity_fm, strlen(identity_fm) + 1U);
        }
        return hu_strndup(alloc, body, body_len);
    }
    if (body_len == 0U) {
        return identity_fm;
    }
    size_t il = strlen(identity_fm);
    size_t total = il + 2U + body_len + 1U;
    char *out = (char *)alloc->alloc(alloc->ctx, total);
    if (!out) {
        alloc->free(alloc->ctx, identity_fm, il + 1U);
        return NULL;
    }
    memcpy(out, identity_fm, il);
    out[il] = '\n';
    out[il + 1U] = '\n';
    memcpy(out + il + 2U, body, body_len);
    out[il + 2U + body_len] = '\0';
    alloc->free(alloc->ctx, identity_fm, il + 1U);
    return out;
}

static hu_error_t parse_markdown_persona(hu_allocator_t *alloc, const char *data, size_t len,
                                         hu_persona_t *out) {
    if (!alloc || !out) {
        return HU_ERR_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));

    const char *fm_start = NULL;
    size_t fm_len = 0;
    const char *body_start = NULL;
    size_t body_len = 0;
    hu_error_t err = split_frontmatter(data, len, &fm_start, &fm_len, &body_start, &body_len);
    if (err != HU_OK) {
        return err;
    }

    char *identity_fm = NULL;
    err = parse_frontmatter(alloc, fm_start, fm_len, out, &identity_fm);
    if (err != HU_OK) {
        hu_persona_deinit(alloc, out);
        memset(out, 0, sizeof(*out));
        return err;
    }

    const char *body_trim = body_start;
    size_t body_trim_len = body_len;
    trim_ws_inplace(&body_trim, &body_trim_len);
    bool had_fm_identity = (identity_fm != NULL);

    out->identity = concat_identity_body(alloc, identity_fm, body_start, body_len);
    if (!out->identity && (body_trim_len > 0U || had_fm_identity)) {
        hu_persona_deinit(alloc, out);
        memset(out, 0, sizeof(*out));
        return HU_ERR_OUT_OF_MEMORY;
    }

    return HU_OK;
}

#if !defined(HU_IS_TEST)
static hu_error_t read_file_contents(hu_allocator_t *alloc, const char *path, char **out_buf,
                                     size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return HU_ERR_IO;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return HU_ERR_IO;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return HU_ERR_IO;
    }
    if (sz > (long)hu_md_max_file_bytes) {
        fclose(f);
        return HU_ERR_PARSE;
    }
    rewind(f);
    char *buf = (char *)alloc->alloc(alloc->ctx, (size_t)sz + 1U);
    if (!buf) {
        fclose(f);
        return HU_ERR_OUT_OF_MEMORY;
    }
    size_t n = fread(buf, 1U, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) {
        alloc->free(alloc->ctx, buf, (size_t)sz + 1U);
        return HU_ERR_IO;
    }
    buf[sz] = '\0';
    *out_buf = buf;
    *out_len = (size_t)sz;
    return HU_OK;
}
#endif

hu_error_t hu_persona_load_markdown(hu_allocator_t *alloc, const char *path, hu_persona_t *out) {
    if (!alloc || !path || !out) {
        return HU_ERR_INVALID_ARGUMENT;
    }
#if defined(HU_IS_TEST)
    (void)path;
    (void)out;
    return HU_ERR_NOT_SUPPORTED;
#else
    char *buf = NULL;
    size_t blen = 0;
    hu_error_t e = read_file_contents(alloc, path, &buf, &blen);
    if (e != HU_OK) {
        return e;
    }
    e = parse_markdown_persona(alloc, buf, blen, out);
    alloc->free(alloc->ctx, buf, blen + 1U);
    if (e != HU_OK) {
        hu_persona_deinit(alloc, out);
        memset(out, 0, sizeof(*out));
    }
    return e;
#endif
}

#if defined(HU_IS_TEST)
hu_error_t hu_persona_load_markdown_buffer(hu_allocator_t *alloc, const char *content,
                                           size_t content_len, hu_persona_t *out) {
    if (!alloc || !out) {
        return HU_ERR_INVALID_ARGUMENT;
    }
    if (content == NULL && content_len > 0U) {
        return HU_ERR_INVALID_ARGUMENT;
    }
    if (content_len == 0U || content == NULL) {
        memset(out, 0, sizeof(*out));
        return HU_OK;
    }
    return parse_markdown_persona(alloc, content, content_len, out);
}
#endif

#if !(defined(HU_IS_TEST) && HU_IS_TEST)
static int md_cmp_cstr(const void *a, const void *b) {
    const char *const *sa = (const char *const *)a;
    const char *const *sb = (const char *const *)b;
    return strcmp(*sa, *sb);
}

#if !(defined(__unix__) || defined(__APPLE__))
static hu_error_t discover_agents_platform(hu_allocator_t *alloc, const char *agents_dir,
                                           char ***out_names, size_t *out_count) {
    (void)alloc;
    (void)agents_dir;
    (void)out_names;
    (void)out_count;
    return HU_ERR_NOT_SUPPORTED;
}
#else
static hu_error_t discover_agents_platform(hu_allocator_t *alloc, const char *agents_dir,
                                           char ***out_names, size_t *out_count) {
    DIR *d = opendir(agents_dir);
    if (!d) {
        return HU_ERR_IO;
    }
    char **names = (char **)alloc->alloc(alloc->ctx, hu_md_max_discover_files * sizeof(char *));
    if (!names) {
        closedir(d);
        return HU_ERR_OUT_OF_MEMORY;
    }
    size_t n = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && n < (size_t)hu_md_max_discover_files) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        size_t nl = strlen(ent->d_name);
        if (nl < 4U || strcmp(ent->d_name + nl - 3U, ".md") != 0) {
            continue;
        }
        size_t plen = strlen(agents_dir) + 1U + nl + 1U;
        char *full = (char *)alloc->alloc(alloc->ctx, plen);
        if (!full) {
            closedir(d);
            for (size_t i = 0; i < n; i++) {
                alloc->free(alloc->ctx, names[i], strlen(names[i]) + 1U);
            }
            alloc->free(alloc->ctx, names, hu_md_max_discover_files * sizeof(char *));
            return HU_ERR_OUT_OF_MEMORY;
        }
        (void)snprintf(full, plen, "%s/%s", agents_dir, ent->d_name);
        FILE *fp = fopen(full, "rb");
        alloc->free(alloc->ctx, full, plen);
        if (!fp) {
            continue;
        }
        char peek[hu_md_peek_bytes];
        size_t got = fread(peek, 1U, sizeof(peek) - 1U, fp);
        fclose(fp);
        peek[got] = '\0';
        const char *fm_s = NULL;
        size_t fm_l = 0;
        const char *b_s = NULL;
        size_t b_l = 0;
        if (split_frontmatter(peek, got, &fm_s, &fm_l, &b_s, &b_l) != HU_OK) {
            continue;
        }
        if (fm_l == 0U) {
            continue;
        }

        hu_persona_t tmp;
        memset(&tmp, 0, sizeof(tmp));
        char *id_fm = NULL;
        if (parse_frontmatter(alloc, fm_s, fm_l, &tmp, &id_fm) != HU_OK) {
            if (id_fm) {
                alloc->free(alloc->ctx, id_fm, strlen(id_fm) + 1U);
            }
            hu_persona_deinit(alloc, &tmp);
            continue;
        }
        const char *label = tmp.name;
        if (!label) {
            /* stem: strip .md */
            const char *base = ent->d_name;
            char stem[256];
            size_t stem_len = nl - 3U;
            if (stem_len >= sizeof(stem)) {
                hu_persona_deinit(alloc, &tmp);
                continue;
            }
            memcpy(stem, base, stem_len);
            stem[stem_len] = '\0';
            label = stem;
        }
        char *copy = hu_strdup(alloc, label);
        hu_persona_deinit(alloc, &tmp);
        if (id_fm) {
            alloc->free(alloc->ctx, id_fm, strlen(id_fm) + 1U);
        }
        if (!copy) {
            closedir(d);
            for (size_t i = 0; i < n; i++) {
                alloc->free(alloc->ctx, names[i], strlen(names[i]) + 1U);
            }
            alloc->free(alloc->ctx, names, hu_md_max_discover_files * sizeof(char *));
            return HU_ERR_OUT_OF_MEMORY;
        }
        names[n++] = copy;
    }
    closedir(d);
    if (n > 1U) {
        qsort(names, n, sizeof(names[0]), md_cmp_cstr);
    }
    *out_names = names;
    *out_count = n;
    return HU_OK;
}
#endif
#endif /* !(HU_IS_TEST) */

hu_error_t hu_persona_discover_agents(hu_allocator_t *alloc, const char *agents_dir,
                                      char ***out_names, size_t *out_count) {
    if (!alloc || !agents_dir || !out_names || !out_count) {
        return HU_ERR_INVALID_ARGUMENT;
    }
    *out_names = NULL;
    *out_count = 0;
#if defined(HU_IS_TEST)
    (void)agents_dir;
    return HU_OK;
#else
    return discover_agents_platform(alloc, agents_dir, out_names, out_count);
#endif
}
