#include "human/persona/persona_fuse.h"
#include "human/core/string.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

hu_error_t hu_persona_fuse_init(hu_persona_fuse_t *fuse, hu_allocator_t *alloc) {
    if (!fuse || !alloc)
        return HU_ERR_INVALID_ARGUMENT;
    memset(fuse, 0, sizeof(*fuse));
    fuse->alloc = alloc;
    return HU_OK;
}

static void free_adapter(hu_allocator_t *alloc, hu_fuse_adapter_t *a) {
    if (a->name)
        alloc->free(alloc->ctx, a->name, a->name_len + 1);
    for (size_t i = 0; i < a->preferred_vocab_count; i++)
        if (a->preferred_vocab[i])
            alloc->free(alloc->ctx, a->preferred_vocab[i], strlen(a->preferred_vocab[i]) + 1);
    for (size_t i = 0; i < a->avoided_vocab_count; i++)
        if (a->avoided_vocab[i])
            alloc->free(alloc->ctx, a->avoided_vocab[i], strlen(a->avoided_vocab[i]) + 1);
}

void hu_persona_fuse_deinit(hu_persona_fuse_t *fuse) {
    if (!fuse)
        return;
    for (size_t i = 0; i < fuse->adapter_count; i++)
        free_adapter(fuse->alloc, &fuse->adapters[i]);
    fuse->adapter_count = 0;
}

hu_error_t hu_persona_fuse_add_adapter(hu_persona_fuse_t *fuse, const char *name, size_t name_len,
                                       float formality, float verbosity, float emoji_factor,
                                       float warmth_offset) {
    if (!fuse || !name || name_len == 0)
        return HU_ERR_INVALID_ARGUMENT;
    if (fuse->adapter_count >= HU_PERSONA_FUSE_MAX_ADAPTERS)
        return HU_ERR_INVALID_ARGUMENT;

    hu_fuse_adapter_t *a = &fuse->adapters[fuse->adapter_count];
    memset(a, 0, sizeof(*a));
    a->name = hu_strndup(fuse->alloc, name, name_len);
    if (!a->name)
        return HU_ERR_OUT_OF_MEMORY;
    a->name_len = name_len;
    a->formality = formality;
    a->verbosity = verbosity;
    a->emoji_factor = emoji_factor;
    a->warmth_offset = warmth_offset;
    fuse->adapter_count++;
    return HU_OK;
}

const hu_fuse_adapter_t *hu_persona_fuse_get(const hu_persona_fuse_t *fuse, const char *name,
                                             size_t name_len) {
    if (!fuse || !name)
        return NULL;
    for (size_t i = 0; i < fuse->adapter_count; i++) {
        if (fuse->adapters[i].name_len == name_len &&
            memcmp(fuse->adapters[i].name, name, name_len) == 0)
            return &fuse->adapters[i];
    }
    return NULL;
}

hu_error_t hu_persona_fuse_compose(const hu_persona_fuse_t *fuse, const char *const *adapter_names,
                                   size_t adapter_count, hu_fuse_result_t *out) {
    if (!fuse || !out)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));
    out->emoji_factor = 1.0f;

    for (size_t i = 0; i < adapter_count; i++) {
        if (!adapter_names[i])
            continue;
        size_t nl = strlen(adapter_names[i]);
        const hu_fuse_adapter_t *a = hu_persona_fuse_get(fuse, adapter_names[i], nl);
        if (!a)
            continue;

        out->formality += a->formality;
        out->verbosity += a->verbosity;
        out->emoji_factor *= a->emoji_factor;
        out->warmth_offset += a->warmth_offset;

        if (a->preferred_vocab_count > 0 && out->preferred_vocab_count == 0) {
            out->preferred_vocab = (const char *const *)a->preferred_vocab;
            out->preferred_vocab_count = a->preferred_vocab_count;
        }
        if (a->avoided_vocab_count > 0 && out->avoided_vocab_count == 0) {
            out->avoided_vocab = (const char *const *)a->avoided_vocab;
            out->avoided_vocab_count = a->avoided_vocab_count;
        }
    }

    /* Clamp values */
    if (out->formality > 1.0f)
        out->formality = 1.0f;
    if (out->formality < -1.0f)
        out->formality = -1.0f;
    if (out->verbosity > 1.0f)
        out->verbosity = 1.0f;
    if (out->verbosity < -1.0f)
        out->verbosity = -1.0f;
    if (out->warmth_offset > 0.5f)
        out->warmth_offset = 0.5f;
    if (out->warmth_offset < -0.5f)
        out->warmth_offset = -0.5f;

    return HU_OK;
}

hu_error_t hu_persona_fuse_add_builtin_adapters(hu_persona_fuse_t *fuse) {
    hu_error_t err;

    err = hu_persona_fuse_add_adapter(fuse, "professional", 12, 0.7f, 0.2f, 0.3f, -0.1f);
    if (err != HU_OK)
        return err;

    err = hu_persona_fuse_add_adapter(fuse, "casual", 6, -0.6f, -0.2f, 1.5f, 0.3f);
    if (err != HU_OK)
        return err;

    err = hu_persona_fuse_add_adapter(fuse, "brief", 5, 0.0f, -0.8f, 0.5f, 0.0f);
    if (err != HU_OK)
        return err;

    err = hu_persona_fuse_add_adapter(fuse, "verbose", 7, 0.0f, 0.8f, 1.0f, 0.0f);
    if (err != HU_OK)
        return err;

    err = hu_persona_fuse_add_adapter(fuse, "empathetic", 10, -0.2f, 0.3f, 1.2f, 0.4f);
    if (err != HU_OK)
        return err;

    return HU_OK;
}

static float hu_clamp11(float x) {
    if (x > 1.0f)
        return 1.0f;
    if (x < -1.0f)
        return -1.0f;
    return x;
}

hu_error_t hu_persona_vector_from_adapter(const hu_persona_fuse_adapter_t *adapter,
                                          hu_persona_vector_t *out) {
    if (!adapter || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->formality = hu_clamp11(adapter->formality);
    out->warmth = hu_clamp11(adapter->warmth_offset * 2.0f);
    out->verbosity = hu_clamp11(adapter->verbosity);
    out->humor = 0.0f;
    out->directness = 0.0f;
    out->emoji_usage = hu_clamp11((adapter->emoji_factor - 1.0f) * 0.85f);
    return HU_OK;
}

hu_error_t hu_persona_vector_compose(const hu_persona_vector_t *vectors, size_t count,
                                     hu_persona_vector_t *out) {
    if (!vectors || count == 0 || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < count; i++) {
        out->formality += vectors[i].formality;
        out->warmth += vectors[i].warmth;
        out->verbosity += vectors[i].verbosity;
        out->humor += vectors[i].humor;
        out->directness += vectors[i].directness;
        out->emoji_usage += vectors[i].emoji_usage;
    }
    float inv = 1.0f / (float)count;
    out->formality = hu_clamp11(out->formality * inv);
    out->warmth = hu_clamp11(out->warmth * inv);
    out->verbosity = hu_clamp11(out->verbosity * inv);
    out->humor = hu_clamp11(out->humor * inv);
    out->directness = hu_clamp11(out->directness * inv);
    out->emoji_usage = hu_clamp11(out->emoji_usage * inv);
    return HU_OK;
}

static void append_dim(char *buf, size_t cap, size_t *pos, const char *sentence) {
    if (!buf || !pos || !sentence || !sentence[0])
        return;
    size_t p = *pos;
    if (p >= cap)
        return;
    int n = snprintf(buf + p, cap - p, "%s%s", p > 0 ? " " : "", sentence);
    if (n > 0 && (size_t)n < cap - p)
        *pos = p + (size_t)n;
}

static void dim_phrase(float v, const char *pos_very, const char *pos_mod, const char *pos_slight,
                       const char *neg_avoid, const char *neg_min, char *out, size_t ocap) {
    if (fabsf(v) <= 0.06f) {
        out[0] = '\0';
        return;
    }
    if (v > 0.55f)
        snprintf(out, ocap, "%s", pos_very);
    else if (v > 0.2f)
        snprintf(out, ocap, "%s", pos_mod);
    else if (v > 0.05f)
        snprintf(out, ocap, "%s", pos_slight);
    else if (v < -0.55f)
        snprintf(out, ocap, "%s", neg_avoid);
    else if (v < -0.15f)
        snprintf(out, ocap, "%s", neg_min);
    else
        out[0] = '\0';
}

hu_error_t hu_persona_vector_to_directive(const hu_persona_vector_t *vec, char *buf,
                                          size_t buf_size, size_t *out_len) {
    if (!vec || !buf || buf_size == 0 || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    buf[0] = '\0';
    *out_len = 0;
    size_t pos = 0;
    char tmp[96];

    dim_phrase(vec->formality, "Be very formal and precise.", "Be moderately formal.",
               "Be slightly formal.", "Avoid stiff formality.", "Minimize overly formal tone.", tmp,
               sizeof(tmp));
    append_dim(buf, buf_size, &pos, tmp);

    dim_phrase(vec->warmth, "Use very warm, empathetic language.", "Use warm, empathetic language.",
               "Lean slightly warm and supportive.", "Avoid excessive warmth.",
               "Minimize sentimental warmth.", tmp, sizeof(tmp));
    append_dim(buf, buf_size, &pos, tmp);

    dim_phrase(vec->verbosity, "Be very detailed and expansive.", "Give moderately rich detail.",
               "Add slight elaboration where helpful.", "Avoid long-winded replies.",
               "Keep responses concise.", tmp, sizeof(tmp));
    append_dim(buf, buf_size, &pos, tmp);

    dim_phrase(vec->humor, "Lean into humor and playfulness.", "Light humor is welcome.",
               "A touch of humor is fine.", "Avoid humor.", "Minimize jokes.", tmp, sizeof(tmp));
    append_dim(buf, buf_size, &pos, tmp);

    dim_phrase(vec->directness, "Be very direct and candid.", "Be direct in recommendations.",
               "Be slightly more direct.", "Avoid bluntness.", "Soften direct recommendations.",
               tmp, sizeof(tmp));
    append_dim(buf, buf_size, &pos, tmp);

    dim_phrase(vec->emoji_usage, "Emoji-rich expression is welcome.", "Moderate emoji is fine.",
               "Use emoji sparingly.", "Avoid emoji.", "Minimal emoji usage.", tmp, sizeof(tmp));
    append_dim(buf, buf_size, &pos, tmp);

    *out_len = pos;
    return HU_OK;
}
