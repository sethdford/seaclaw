#include "human/persona/persona_fuse.h"
#include "human/core/string.h"
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
