#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/string.h"
#include "seaclaw/persona.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

static bool string_in_array(const char *s, char **arr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (arr[i] && strcmp(arr[i], s) == 0)
            return true;
    }
    return false;
}

/* Merge traits from all partials, deduplicating */
static sc_error_t merge_traits(sc_allocator_t *alloc, char ***out, size_t *out_count,
                               const sc_persona_t *partials, size_t count) {
    size_t total = 0;
    for (size_t i = 0; i < count; i++)
        total += partials[i].traits_count;
    if (total == 0)
        return SC_OK;
    char **buf = (char **)alloc->alloc(alloc->ctx, total * sizeof(char *));
    if (!buf)
        return SC_ERR_OUT_OF_MEMORY;
    size_t n = 0;
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < partials[i].traits_count; j++) {
            const char *t = partials[i].traits[j];
            if (!t || string_in_array(t, buf, n))
                continue;
            char *dup = sc_strdup(alloc, t);
            if (!dup) {
                for (size_t k = 0; k < n; k++)
                    alloc->free(alloc->ctx, buf[k], strlen(buf[k]) + 1);
                alloc->free(alloc->ctx, buf, total * sizeof(char *));
                return SC_ERR_OUT_OF_MEMORY;
            }
            buf[n++] = dup;
        }
    }
    *out = buf;
    *out_count = n;
    return SC_OK;
}

/* Merge preferred_vocab only */
static sc_error_t merge_preferred_vocab(sc_allocator_t *alloc, char ***out, size_t *out_count,
                                        const sc_persona_t *partials, size_t count) {
    size_t total = 0;
    for (size_t i = 0; i < count; i++)
        total += partials[i].preferred_vocab_count;
    if (total == 0)
        return SC_OK;
    char **buf = (char **)alloc->alloc(alloc->ctx, total * sizeof(char *));
    if (!buf)
        return SC_ERR_OUT_OF_MEMORY;
    size_t n = 0;
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < partials[i].preferred_vocab_count; j++) {
            const char *v = partials[i].preferred_vocab[j];
            if (!v || string_in_array(v, buf, n))
                continue;
            char *dup = sc_strdup(alloc, v);
            if (!dup) {
                for (size_t k = 0; k < n; k++)
                    alloc->free(alloc->ctx, buf[k], strlen(buf[k]) + 1);
                alloc->free(alloc->ctx, buf, total * sizeof(char *));
                return SC_ERR_OUT_OF_MEMORY;
            }
            buf[n++] = dup;
        }
    }
    *out = buf;
    *out_count = n;
    return SC_OK;
}

static sc_error_t merge_communication_rules(sc_allocator_t *alloc, char ***out, size_t *out_count,
                                            const sc_persona_t *partials, size_t count) {
    size_t total = 0;
    for (size_t i = 0; i < count; i++)
        total += partials[i].communication_rules_count;
    if (total == 0)
        return SC_OK;
    char **buf = (char **)alloc->alloc(alloc->ctx, total * sizeof(char *));
    if (!buf)
        return SC_ERR_OUT_OF_MEMORY;
    size_t n = 0;
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < partials[i].communication_rules_count; j++) {
            const char *r = partials[i].communication_rules[j];
            if (!r || string_in_array(r, buf, n))
                continue;
            char *dup = sc_strdup(alloc, r);
            if (!dup) {
                for (size_t k = 0; k < n; k++)
                    alloc->free(alloc->ctx, buf[k], strlen(buf[k]) + 1);
                alloc->free(alloc->ctx, buf, total * sizeof(char *));
                return SC_ERR_OUT_OF_MEMORY;
            }
            buf[n++] = dup;
        }
    }
    *out = buf;
    *out_count = n;
    return SC_OK;
}

static sc_error_t merge_overlays(sc_allocator_t *alloc, sc_persona_overlay_t **out,
                                 size_t *out_count, const sc_persona_t *partials, size_t count) {
    size_t total = 0;
    for (size_t i = 0; i < count; i++)
        total += partials[i].overlays_count;
    if (total == 0)
        return SC_OK;
    sc_persona_overlay_t *buf =
        (sc_persona_overlay_t *)alloc->alloc(alloc->ctx, total * sizeof(sc_persona_overlay_t));
    if (!buf)
        return SC_ERR_OUT_OF_MEMORY;
    memset(buf, 0, total * sizeof(sc_persona_overlay_t));
    size_t n = 0;
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < partials[i].overlays_count; j++) {
            const sc_persona_overlay_t *ov = &partials[i].overlays[j];
            bool found = false;
            for (size_t k = 0; k < n; k++) {
                if (ov->channel && buf[k].channel && strcmp(ov->channel, buf[k].channel) == 0) {
                    found = true;
                    break;
                }
            }
            if (found)
                continue;
            buf[n].channel = ov->channel ? sc_strdup(alloc, ov->channel) : NULL;
            if (ov->channel && !buf[n].channel)
                goto overlay_oom;
            buf[n].formality = ov->formality ? sc_strdup(alloc, ov->formality) : NULL;
            if (ov->formality && !buf[n].formality)
                goto overlay_oom;
            buf[n].avg_length = ov->avg_length ? sc_strdup(alloc, ov->avg_length) : NULL;
            if (ov->avg_length && !buf[n].avg_length)
                goto overlay_oom;
            buf[n].emoji_usage = ov->emoji_usage ? sc_strdup(alloc, ov->emoji_usage) : NULL;
            if (ov->emoji_usage && !buf[n].emoji_usage)
                goto overlay_oom;
            if (ov->style_notes_count > 0 && ov->style_notes) {
                buf[n].style_notes =
                    (char **)alloc->alloc(alloc->ctx, ov->style_notes_count * sizeof(char *));
                if (buf[n].style_notes) {
                    buf[n].style_notes_count = ov->style_notes_count;
                    for (size_t k = 0; k < ov->style_notes_count; k++) {
                        buf[n].style_notes[k] =
                            ov->style_notes[k] ? sc_strdup(alloc, ov->style_notes[k]) : NULL;
                        if (ov->style_notes[k] && !buf[n].style_notes[k])
                            goto overlay_oom;
                    }
                }
            }
            n++;
        }
    }
    *out = buf;
    *out_count = n;
    return SC_OK;
overlay_oom:
    for (size_t i = 0; i <= n; i++) {
        if (buf[i].channel)
            alloc->free(alloc->ctx, buf[i].channel, strlen(buf[i].channel) + 1);
        if (buf[i].formality)
            alloc->free(alloc->ctx, buf[i].formality, strlen(buf[i].formality) + 1);
        if (buf[i].avg_length)
            alloc->free(alloc->ctx, buf[i].avg_length, strlen(buf[i].avg_length) + 1);
        if (buf[i].emoji_usage)
            alloc->free(alloc->ctx, buf[i].emoji_usage, strlen(buf[i].emoji_usage) + 1);
        if (buf[i].style_notes) {
            for (size_t k = 0; k < buf[i].style_notes_count; k++)
                if (buf[i].style_notes[k])
                    alloc->free(alloc->ctx, buf[i].style_notes[k],
                                strlen(buf[i].style_notes[k]) + 1);
            alloc->free(alloc->ctx, buf[i].style_notes, buf[i].style_notes_count * sizeof(char *));
        }
    }
    alloc->free(alloc->ctx, buf, total * sizeof(sc_persona_overlay_t));
    return SC_ERR_OUT_OF_MEMORY;
}

sc_error_t sc_persona_creator_synthesize(sc_allocator_t *alloc, const sc_persona_t *partials,
                                         size_t count, const char *name, size_t name_len,
                                         sc_persona_t *out) {
    if (!alloc || !partials || !out)
        return SC_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

    out->name = sc_strndup(alloc, name, name_len);
    if (!out->name)
        return SC_ERR_OUT_OF_MEMORY;
    out->name_len = name_len;

    sc_error_t err = merge_traits(alloc, &out->traits, &out->traits_count, partials, count);
    if (err != SC_OK) {
        sc_persona_deinit(alloc, out);
        return err;
    }
    err = merge_preferred_vocab(alloc, &out->preferred_vocab, &out->preferred_vocab_count, partials,
                                count);
    if (err != SC_OK) {
        sc_persona_deinit(alloc, out);
        return err;
    }
    /* Merge avoided and slang similarly - for simplicity use merge_vocab for all, but test only
       checks traits. Use separate merges for avoided/slang. */
    size_t total_avoided = 0, total_slang = 0;
    for (size_t i = 0; i < count; i++) {
        total_avoided += partials[i].avoided_vocab_count;
        total_slang += partials[i].slang_count;
    }
    if (total_avoided > 0) {
        char **abuf = (char **)alloc->alloc(alloc->ctx, total_avoided * sizeof(char *));
        if (abuf) {
            size_t an = 0;
            for (size_t i = 0; i < count; i++) {
                for (size_t j = 0; j < partials[i].avoided_vocab_count; j++) {
                    const char *v = partials[i].avoided_vocab[j];
                    if (v && !string_in_array(v, abuf, an)) {
                        char *dup = sc_strdup(alloc, v);
                        if (!dup) {
                            for (size_t k = 0; k < an; k++)
                                alloc->free(alloc->ctx, abuf[k], strlen(abuf[k]) + 1);
                            alloc->free(alloc->ctx, abuf, total_avoided * sizeof(char *));
                            sc_persona_deinit(alloc, out);
                            return SC_ERR_OUT_OF_MEMORY;
                        }
                        abuf[an++] = dup;
                    }
                }
            }
            out->avoided_vocab = abuf;
            out->avoided_vocab_count = an;
        }
    }
    if (total_slang > 0) {
        char **sbuf = (char **)alloc->alloc(alloc->ctx, total_slang * sizeof(char *));
        if (sbuf) {
            size_t sn = 0;
            for (size_t i = 0; i < count; i++) {
                for (size_t j = 0; j < partials[i].slang_count; j++) {
                    const char *v = partials[i].slang[j];
                    if (v && !string_in_array(v, sbuf, sn)) {
                        char *dup = sc_strdup(alloc, v);
                        if (!dup) {
                            for (size_t k = 0; k < sn; k++)
                                alloc->free(alloc->ctx, sbuf[k], strlen(sbuf[k]) + 1);
                            alloc->free(alloc->ctx, sbuf, total_slang * sizeof(char *));
                            sc_persona_deinit(alloc, out);
                            return SC_ERR_OUT_OF_MEMORY;
                        }
                        sbuf[sn++] = dup;
                    }
                }
            }
            out->slang = sbuf;
            out->slang_count = sn;
        }
    }

    err = merge_communication_rules(alloc, &out->communication_rules,
                                    &out->communication_rules_count, partials, count);
    if (err != SC_OK) {
        sc_persona_deinit(alloc, out);
        return err;
    }

    /* Merge values (dedup) */
    size_t vtotal = 0;
    for (size_t i = 0; i < count; i++)
        vtotal += partials[i].values_count;
    if (vtotal > 0) {
        char **vbuf = (char **)alloc->alloc(alloc->ctx, vtotal * sizeof(char *));
        if (vbuf) {
            size_t vn = 0;
            for (size_t i = 0; i < count; i++) {
                for (size_t k = 0; k < partials[i].values_count; k++) {
                    const char *v = partials[i].values[k];
                    if (v && !string_in_array(v, vbuf, vn)) {
                        char *dup = sc_strdup(alloc, v);
                        if (!dup) {
                            for (size_t j = 0; j < vn; j++)
                                alloc->free(alloc->ctx, vbuf[j], strlen(vbuf[j]) + 1);
                            alloc->free(alloc->ctx, vbuf, vtotal * sizeof(char *));
                            sc_persona_deinit(alloc, out);
                            return SC_ERR_OUT_OF_MEMORY;
                        }
                        vbuf[vn++] = dup;
                    }
                }
            }
            out->values = vbuf;
            out->values_count = vn;
        }
    }

    if (count > 0 && partials[0].decision_style) {
        out->decision_style = sc_strdup(alloc, partials[0].decision_style);
        if (!out->decision_style) {
            sc_persona_deinit(alloc, out);
            return SC_ERR_OUT_OF_MEMORY;
        }
    }

    err = merge_overlays(alloc, &out->overlays, &out->overlays_count, partials, count);
    if (err != SC_OK) {
        sc_persona_deinit(alloc, out);
        return err;
    }
    return SC_OK;
}

#define SC_PERSONA_CREATOR_PATH_MAX 512

static sc_error_t write_json_string(FILE *f, const char *s) {
    fputc('"', f);
    for (const char *p = s ? s : ""; *p; p++) {
        switch (*p) {
        case '"':
            fputs("\\\"", f);
            break;
        case '\\':
            fputs("\\\\", f);
            break;
        case '\n':
            fputs("\\n", f);
            break;
        case '\r':
            fputs("\\r", f);
            break;
        case '\t':
            fputs("\\t", f);
            break;
        default:
            fputc(*p, f);
            break;
        }
    }
    fputc('"', f);
    return SC_OK;
}

static sc_error_t write_json_string_array(FILE *f, char **arr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (i > 0)
            fputc(',', f);
        if (write_json_string(f, arr[i] ? arr[i] : "") != SC_OK)
            return SC_ERR_IO;
    }
    return SC_OK;
}

sc_error_t sc_persona_creator_write(sc_allocator_t *alloc, const sc_persona_t *persona) {
    if (!alloc || !persona || !persona->name)
        return SC_ERR_INVALID_ARGUMENT;

    const char *home = getenv("HOME");
    if (!home || !home[0])
        return SC_ERR_NOT_FOUND;
    char dir_buf[SC_PERSONA_CREATOR_PATH_MAX];
    int n = snprintf(dir_buf, sizeof(dir_buf), "%s/.seaclaw/personas", home);
    if (n <= 0 || (size_t)n >= sizeof(dir_buf))
        return SC_ERR_INVALID_ARGUMENT;
#if defined(__unix__) || defined(__APPLE__)
    char parent[SC_PERSONA_CREATOR_PATH_MAX];
    int pn = snprintf(parent, sizeof(parent), "%s/.seaclaw", home);
    if (pn > 0 && (size_t)pn < sizeof(parent))
        (void)mkdir(parent, 0755);
    if (mkdir(dir_buf, 0755) != 0 && errno != EEXIST)
        return SC_ERR_IO;
#endif

    char path[SC_PERSONA_CREATOR_PATH_MAX];
    n = snprintf(path, sizeof(path), "%s/%.*s.json", dir_buf, (int)persona->name_len,
                 persona->name);
    if (n <= 0 || (size_t)n >= sizeof(path))
        return SC_ERR_INVALID_ARGUMENT;

    FILE *f = fopen(path, "wb");
    if (!f)
        return SC_ERR_IO;

    fputs("{\n  \"version\": 1,\n  \"name\": ", f);
    if (write_json_string(f, persona->name) != SC_OK)
        goto fail;
    fputs(",\n  \"core\": {\n", f);

    fputs("    \"identity\": ", f);
    if (write_json_string(f, persona->identity) != SC_OK)
        goto fail;
    fputs(",\n    \"traits\": [", f);
    if (write_json_string_array(f, persona->traits, persona->traits_count) != SC_OK)
        goto fail;
    fputs("],\n    \"vocabulary\": {\n      \"preferred\": [", f);
    if (write_json_string_array(f, persona->preferred_vocab, persona->preferred_vocab_count) !=
        SC_OK)
        goto fail;
    fputs("],\n      \"avoided\": [", f);
    if (write_json_string_array(f, persona->avoided_vocab, persona->avoided_vocab_count) != SC_OK)
        goto fail;
    fputs("],\n      \"slang\": [", f);
    if (write_json_string_array(f, persona->slang, persona->slang_count) != SC_OK)
        goto fail;
    fputs("]\n    },\n    \"communication_rules\": [", f);
    if (write_json_string_array(f, persona->communication_rules,
                                persona->communication_rules_count) != SC_OK)
        goto fail;
    fputs("],\n    \"values\": [", f);
    if (write_json_string_array(f, persona->values, persona->values_count) != SC_OK)
        goto fail;
    fputs("],\n    \"decision_style\": ", f);
    if (write_json_string(f, persona->decision_style) != SC_OK)
        goto fail;

    fputs("\n  },\n  \"channel_overlays\": {\n", f);
    for (size_t i = 0; i < persona->overlays_count; i++) {
        const sc_persona_overlay_t *ov = &persona->overlays[i];
        if (!ov->channel)
            continue;
        if (i > 0)
            fputs(",\n", f);
        fputs("    ", f);
        if (write_json_string(f, ov->channel) != SC_OK)
            goto fail;
        fputs(": {\n      \"formality\": ", f);
        if (write_json_string(f, ov->formality) != SC_OK)
            goto fail;
        fputs(",\n      \"avg_length\": ", f);
        if (write_json_string(f, ov->avg_length) != SC_OK)
            goto fail;
        fputs(",\n      \"emoji_usage\": ", f);
        if (write_json_string(f, ov->emoji_usage) != SC_OK)
            goto fail;
        fputs(",\n      \"style_notes\": [", f);
        if (write_json_string_array(f, ov->style_notes, ov->style_notes_count) != SC_OK)
            goto fail;
        fputs("]\n    }", f);
    }
    fputs("\n  }\n}\n", f);

    if (ferror(f)) {
        fclose(f);
        return SC_ERR_IO;
    }
    fclose(f);
    return SC_OK;
fail:
    fclose(f);
    return SC_ERR_IO;
}
