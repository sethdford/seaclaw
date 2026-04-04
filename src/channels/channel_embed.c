#include "human/channels/channel_embed.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    hu_allocator_t *alloc;
    char *buf;
    size_t cap;
    size_t len;
} hu_json_buf_t;

static void hu_json_buf_free(hu_json_buf_t *j) {
    if (!j || !j->buf || !j->alloc || !j->alloc->free)
        return;
    j->alloc->free(j->alloc->ctx, j->buf, j->cap);
    j->buf = NULL;
    j->cap = 0;
    j->len = 0;
}

static hu_error_t hu_json_buf_reserve(hu_json_buf_t *j, size_t extra) {
    size_t need;
    if (SIZE_MAX - j->len < extra + 1)
        return HU_ERR_OUT_OF_MEMORY;
    need = j->len + extra + 1;
    if (need <= j->cap)
        return HU_OK;
    {
        size_t ncap = j->cap ? j->cap : 2048;
        while (ncap < need) {
            if (ncap > SIZE_MAX / 2)
                return HU_ERR_OUT_OF_MEMORY;
            ncap *= 2;
        }
        {
            void *p = j->alloc->realloc(j->alloc->ctx, j->buf, j->cap, ncap);
            if (!p)
                return HU_ERR_OUT_OF_MEMORY;
            j->buf = (char *)p;
            j->cap = ncap;
        }
    }
    return HU_OK;
}

static hu_error_t hu_json_buf_append(hu_json_buf_t *j, const char *s, size_t n) {
    hu_error_t e = hu_json_buf_reserve(j, n);
    if (e != HU_OK)
        return e;
    memcpy(j->buf + j->len, s, n);
    j->len += n;
    j->buf[j->len] = '\0';
    return HU_OK;
}

static hu_error_t hu_json_buf_append_cstr(hu_json_buf_t *j, const char *s) {
    return hu_json_buf_append(j, s, strlen(s));
}

static hu_error_t hu_json_buf_append_json_str(hu_json_buf_t *j, const char *s) {
    hu_error_t e = hu_json_buf_append(j, "\"", 1);
    if (e != HU_OK)
        return e;
    if (!s)
        return hu_json_buf_append(j, "\"", 1);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        char esc[8];
        size_t elen;
        switch (*p) {
        case '\\':
            esc[0] = '\\';
            esc[1] = '\\';
            elen = 2;
            break;
        case '"':
            esc[0] = '\\';
            esc[1] = '"';
            elen = 2;
            break;
        case '\n':
            esc[0] = '\\';
            esc[1] = 'n';
            elen = 2;
            break;
        case '\r':
            esc[0] = '\\';
            esc[1] = 'r';
            elen = 2;
            break;
        case '\t':
            esc[0] = '\\';
            esc[1] = 't';
            elen = 2;
            break;
        default:
            if (*p < 32u) {
                (void)snprintf(esc, sizeof(esc), "\\u%04x", (unsigned)*p);
                elen = strlen(esc);
            } else {
                esc[0] = (char)*p;
                elen = 1;
            }
            break;
        }
        e = hu_json_buf_append(j, esc, elen);
        if (e != HU_OK)
            return e;
    }
    return hu_json_buf_append(j, "\"", 1);
}

static hu_error_t hu_json_buf_append_u32(hu_json_buf_t *j, uint32_t v) {
    char tmp[16];
    int n = snprintf(tmp, sizeof(tmp), "%" PRIu32, v);
    if (n < 0 || (size_t)n >= sizeof(tmp))
        return HU_ERR_IO;
    return hu_json_buf_append(j, tmp, (size_t)n);
}

static hu_error_t hu_json_buf_finish(hu_json_buf_t *j, char **out, size_t *out_len) {
    size_t final = j->len + 1;
    if (j->cap != final) {
        void *p = j->alloc->realloc(j->alloc->ctx, j->buf, j->cap, final);
        if (!p)
            return HU_ERR_OUT_OF_MEMORY;
        j->buf = (char *)p;
        j->cap = final;
    }
    j->buf[j->len] = '\0';
    *out = j->buf;
    *out_len = j->len;
    j->buf = NULL;
    j->cap = 0;
    j->len = 0;
    return HU_OK;
}

static hu_error_t hu_embed_append_telegram_text(hu_json_buf_t *j, const hu_embed_t *e,
                                                hu_allocator_t *alloc) {
    size_t tlen = e->title ? strlen(e->title) : 0;
    size_t dlen = e->description ? strlen(e->description) : 0;
    size_t cap = tlen + dlen + 8;
    char *tmp = (char *)alloc->alloc(alloc->ctx, cap);
    if (!tmp)
        return HU_ERR_OUT_OF_MEMORY;
    {
        size_t pos = 0;
        if (e->title) {
            tmp[pos++] = '*';
            memcpy(tmp + pos, e->title, tlen);
            pos += tlen;
            tmp[pos++] = '*';
        }
        if (e->title && e->description)
            tmp[pos++] = '\n';
        if (e->description) {
            memcpy(tmp + pos, e->description, dlen);
            pos += dlen;
        }
        tmp[pos] = '\0';
        {
            hu_error_t err = hu_json_buf_append_json_str(j, tmp);
            alloc->free(alloc->ctx, tmp, cap);
            return err;
        }
    }
}

static bool hu_embed_validate_fmt_args(hu_allocator_t *alloc, const hu_embed_t *embed, char **out,
                                       size_t *out_len) {
    (void)embed;
    if (!alloc || !alloc->alloc || !alloc->realloc || !alloc->free)
        return false;
    if (!embed || !out || !out_len)
        return false;
    return true;
}

hu_error_t hu_embed_format_discord(hu_allocator_t *alloc, const hu_embed_t *embed, char **out,
                                   size_t *out_len) {
    hu_json_buf_t j = {0};
    hu_error_t err;
    if (!hu_embed_validate_fmt_args(alloc, embed, out, out_len))
        return HU_ERR_INVALID_ARGUMENT;
    j.alloc = alloc;
    err = hu_json_buf_reserve(&j, 2048);
    if (err != HU_OK)
        return err;
    err = hu_json_buf_append_cstr(&j, "{\"embeds\":[{\"");
    if (err != HU_OK)
        goto fail;
    {
        bool need_comma = false;
        if (embed->title) {
            err = hu_json_buf_append_cstr(&j, "\"title\":");
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_json_str(&j, embed->title);
            if (err != HU_OK)
                goto fail;
            need_comma = true;
        }
        if (embed->description) {
            if (need_comma) {
                err = hu_json_buf_append_cstr(&j, ",");
                if (err != HU_OK)
                    goto fail;
            }
            err = hu_json_buf_append_cstr(&j, "\"description\":");
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_json_str(&j, embed->description);
            if (err != HU_OK)
                goto fail;
            need_comma = true;
        }
        if (need_comma) {
            err = hu_json_buf_append_cstr(&j, ",");
            if (err != HU_OK)
                goto fail;
        }
        err = hu_json_buf_append_cstr(&j, "\"color\":");
        if (err != HU_OK)
            goto fail;
        err = hu_json_buf_append_u32(&j, embed->color);
        if (err != HU_OK)
            goto fail;
        if (embed->image_url) {
            err = hu_json_buf_append_cstr(&j, ",\"image\":{\"url\":");
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_json_str(&j, embed->image_url);
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_cstr(&j, "}");
            if (err != HU_OK)
                goto fail;
        }
        if (embed->thumbnail_url) {
            err = hu_json_buf_append_cstr(&j, ",\"thumbnail\":{\"url\":");
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_json_str(&j, embed->thumbnail_url);
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_cstr(&j, "}");
            if (err != HU_OK)
                goto fail;
        }
        if (embed->footer) {
            err = hu_json_buf_append_cstr(&j, ",\"footer\":{\"text\":");
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_json_str(&j, embed->footer);
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_cstr(&j, "}");
            if (err != HU_OK)
                goto fail;
        }
    }
    err = hu_json_buf_append_cstr(&j, "}],\"components\":[");
    if (err != HU_OK)
        goto fail;
    if (embed->button_count > 0) {
        err = hu_json_buf_append_cstr(&j, "{\"type\":1,\"components\":[");
        if (err != HU_OK)
            goto fail;
        {
            size_t i;
            bool first_btn = true;
            for (i = 0; i < embed->button_count && i < HU_EMBED_MAX_BUTTONS; i++) {
                const hu_embed_button_t *b = &embed->buttons[i];
                if (!b->label || !b->url)
                    continue;
                if (!first_btn) {
                    err = hu_json_buf_append_cstr(&j, ",");
                    if (err != HU_OK)
                        goto fail;
                }
                err = hu_json_buf_append_cstr(&j, "{\"type\":2,\"style\":5,\"label\":");
                if (err != HU_OK)
                    goto fail;
                err = hu_json_buf_append_json_str(&j, b->label);
                if (err != HU_OK)
                    goto fail;
                err = hu_json_buf_append_cstr(&j, ",\"url\":");
                if (err != HU_OK)
                    goto fail;
                err = hu_json_buf_append_json_str(&j, b->url);
                if (err != HU_OK)
                    goto fail;
                err = hu_json_buf_append_cstr(&j, "}");
                if (err != HU_OK)
                    goto fail;
            }
        }
        err = hu_json_buf_append_cstr(&j, "]}");
        if (err != HU_OK)
            goto fail;
    }
    err = hu_json_buf_append_cstr(&j, "]}");
    if (err != HU_OK)
        goto fail;
    return hu_json_buf_finish(&j, out, out_len);
fail:
    hu_json_buf_free(&j);
    return err;
}

hu_error_t hu_embed_format_slack(hu_allocator_t *alloc, const hu_embed_t *embed, char **out,
                                 size_t *out_len) {
    hu_json_buf_t j = {0};
    hu_error_t err;
    if (!hu_embed_validate_fmt_args(alloc, embed, out, out_len))
        return HU_ERR_INVALID_ARGUMENT;
    j.alloc = alloc;
    err = hu_json_buf_reserve(&j, 2048);
    if (err != HU_OK)
        return err;
    err = hu_json_buf_append_cstr(&j, "{\"blocks\":[");
    if (err != HU_OK)
        goto fail;
    {
        bool first = true;
        if (embed->title || embed->description) {
            err = hu_json_buf_append_cstr(&j,
                                          "{\"type\":\"section\",\"text\":{\"type\":\"mrkdwn\","
                                          "\"text\":");
            if (err != HU_OK)
                goto fail;
            {
                size_t tlen = embed->title ? strlen(embed->title) : 0;
                size_t dlen = embed->description ? strlen(embed->description) : 0;
                size_t cap = tlen + dlen + 8;
                char *tmp = (char *)alloc->alloc(alloc->ctx, cap);
                if (!tmp) {
                    err = HU_ERR_OUT_OF_MEMORY;
                    goto fail;
                }
                {
                    size_t pos = 0;
                    if (embed->title) {
                        tmp[pos++] = '*';
                        memcpy(tmp + pos, embed->title, tlen);
                        pos += tlen;
                        tmp[pos++] = '*';
                    }
                    if (embed->title && embed->description)
                        tmp[pos++] = '\n';
                    if (embed->description) {
                        memcpy(tmp + pos, embed->description, dlen);
                        pos += dlen;
                    }
                    tmp[pos] = '\0';
                    err = hu_json_buf_append_json_str(&j, tmp);
                    alloc->free(alloc->ctx, tmp, cap);
                    if (err != HU_OK)
                        goto fail;
                }
            }
            err = hu_json_buf_append_cstr(&j, "}}");
            if (err != HU_OK)
                goto fail;
            first = false;
        }
        if (embed->image_url) {
            if (!first) {
                err = hu_json_buf_append_cstr(&j, ",");
                if (err != HU_OK)
                    goto fail;
            }
            err = hu_json_buf_append_cstr(&j, "{\"type\":\"image\",\"image_url\":");
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_json_str(&j, embed->image_url);
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_cstr(&j, ",\"alt_text\":");
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_json_str(&j, embed->title ? embed->title : "image");
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_cstr(&j, "}");
            if (err != HU_OK)
                goto fail;
            first = false;
        }
        if (embed->footer) {
            if (!first) {
                err = hu_json_buf_append_cstr(&j, ",");
                if (err != HU_OK)
                    goto fail;
            }
            err = hu_json_buf_append_cstr(
                &j, "{\"type\":\"context\",\"elements\":[{\"type\":\"mrkdwn\",\"text\":");
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_json_str(&j, embed->footer);
            if (err != HU_OK)
                goto fail;
            err = hu_json_buf_append_cstr(&j, "}]}");
            if (err != HU_OK)
                goto fail;
            first = false;
        }
        if (embed->button_count > 0) {
            if (!first) {
                err = hu_json_buf_append_cstr(&j, ",");
                if (err != HU_OK)
                    goto fail;
            }
            err = hu_json_buf_append_cstr(&j, "{\"type\":\"actions\",\"elements\":[");
            if (err != HU_OK)
                goto fail;
            {
                size_t i;
                bool btn_first = true;
                for (i = 0; i < embed->button_count && i < HU_EMBED_MAX_BUTTONS; i++) {
                    const hu_embed_button_t *b = &embed->buttons[i];
                    if (!b->label || !b->url)
                        continue;
                    if (!btn_first) {
                        err = hu_json_buf_append_cstr(&j, ",");
                        if (err != HU_OK)
                            goto fail;
                    }
                    err = hu_json_buf_append_cstr(
                        &j, "{\"type\":\"button\",\"text\":{\"type\":\"plain_text\",\"text\":");
                    if (err != HU_OK)
                        goto fail;
                    err = hu_json_buf_append_json_str(&j, b->label);
                    if (err != HU_OK)
                        goto fail;
                    err = hu_json_buf_append_cstr(&j, "},\"url\":");
                    if (err != HU_OK)
                        goto fail;
                    err = hu_json_buf_append_json_str(&j, b->url);
                    if (err != HU_OK)
                        goto fail;
                    err = hu_json_buf_append_cstr(&j, "}");
                    if (err != HU_OK)
                        goto fail;
                    btn_first = false;
                }
            }
            err = hu_json_buf_append_cstr(&j, "]}");
            if (err != HU_OK)
                goto fail;
        }
    }
    err = hu_json_buf_append_cstr(&j, "]}");
    if (err != HU_OK)
        goto fail;
    return hu_json_buf_finish(&j, out, out_len);
fail:
    hu_json_buf_free(&j);
    return err;
}

hu_error_t hu_embed_format_telegram(hu_allocator_t *alloc, const hu_embed_t *embed, char **out,
                                    size_t *out_len) {
    hu_json_buf_t j = {0};
    hu_error_t err;
    if (!hu_embed_validate_fmt_args(alloc, embed, out, out_len))
        return HU_ERR_INVALID_ARGUMENT;
    j.alloc = alloc;
    err = hu_json_buf_reserve(&j, 2048);
    if (err != HU_OK)
        return err;
    err = hu_json_buf_append_cstr(&j, "{\"text\":");
    if (err != HU_OK)
        goto fail;
    err = hu_embed_append_telegram_text(&j, embed, alloc);
    if (err != HU_OK)
        goto fail;
    err = hu_json_buf_append_cstr(&j, ",\"parse_mode\":\"Markdown\",\"reply_markup\":{\"inline_keyboard\":[");
    if (err != HU_OK)
        goto fail;
    if (embed->button_count > 0) {
        err = hu_json_buf_append_cstr(&j, "[");
        if (err != HU_OK)
            goto fail;
        {
            size_t i;
            for (i = 0; i < embed->button_count && i < HU_EMBED_MAX_BUTTONS; i++) {
                const hu_embed_button_t *b = &embed->buttons[i];
                if (!b->label || !b->url)
                    continue;
                if (i > 0) {
                    err = hu_json_buf_append_cstr(&j, ",");
                    if (err != HU_OK)
                        goto fail;
                }
                err = hu_json_buf_append_cstr(&j, "{\"text\":");
                if (err != HU_OK)
                    goto fail;
                err = hu_json_buf_append_json_str(&j, b->label);
                if (err != HU_OK)
                    goto fail;
                err = hu_json_buf_append_cstr(&j, ",\"url\":");
                if (err != HU_OK)
                    goto fail;
                err = hu_json_buf_append_json_str(&j, b->url);
                if (err != HU_OK)
                    goto fail;
                err = hu_json_buf_append_cstr(&j, "}");
                if (err != HU_OK)
                    goto fail;
            }
        }
        err = hu_json_buf_append_cstr(&j, "]");
        if (err != HU_OK)
            goto fail;
    }
    err = hu_json_buf_append_cstr(&j, "]}}");
    if (err != HU_OK)
        goto fail;
    return hu_json_buf_finish(&j, out, out_len);
fail:
    hu_json_buf_free(&j);
    return err;
}

void hu_embed_deinit(hu_embed_t *embed, hu_allocator_t *alloc) {
    size_t i;
    if (!embed || !alloc || !alloc->free)
        return;
    if (embed->title) {
        alloc->free(alloc->ctx, embed->title, strlen(embed->title) + 1);
        embed->title = NULL;
    }
    if (embed->description) {
        alloc->free(alloc->ctx, embed->description, strlen(embed->description) + 1);
        embed->description = NULL;
    }
    if (embed->image_url) {
        alloc->free(alloc->ctx, embed->image_url, strlen(embed->image_url) + 1);
        embed->image_url = NULL;
    }
    if (embed->thumbnail_url) {
        alloc->free(alloc->ctx, embed->thumbnail_url, strlen(embed->thumbnail_url) + 1);
        embed->thumbnail_url = NULL;
    }
    if (embed->footer) {
        alloc->free(alloc->ctx, embed->footer, strlen(embed->footer) + 1);
        embed->footer = NULL;
    }
    for (i = 0; i < embed->button_count && i < HU_EMBED_MAX_BUTTONS; i++) {
        if (embed->buttons[i].label) {
            alloc->free(alloc->ctx, embed->buttons[i].label, strlen(embed->buttons[i].label) + 1);
            embed->buttons[i].label = NULL;
        }
        if (embed->buttons[i].url) {
            alloc->free(alloc->ctx, embed->buttons[i].url, strlen(embed->buttons[i].url) + 1);
            embed->buttons[i].url = NULL;
        }
    }
    embed->button_count = 0;
}
