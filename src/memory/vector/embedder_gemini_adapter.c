#include "seaclaw/memory/vector/embedder_gemini_adapter.h"
#include <string.h>

typedef struct gemini_adapter_ctx {
    sc_embedding_provider_t provider;
} gemini_adapter_ctx_t;

static sc_error_t embed_impl(void *ctx, sc_allocator_t *alloc, const char *text, size_t text_len,
                             sc_embedding_t *out) {
    gemini_adapter_ctx_t *adapter = (gemini_adapter_ctx_t *)ctx;
    if (!adapter || !adapter->provider.vtable || !alloc || !out)
        return SC_ERR_INVALID_ARGUMENT;

    if (text_len > 0 && !text)
        return SC_ERR_INVALID_ARGUMENT;

    sc_embedding_provider_result_t result = {0};
    sc_error_t err =
        adapter->provider.vtable->embed(adapter->provider.ctx, alloc, text, text_len, &result);
    if (err != SC_OK)
        return err;

    if (result.dimensions == 0) {
        sc_embedding_provider_free(alloc, &result);
        out->values = NULL;
        out->dim = 0;
        return SC_OK;
    }

    out->values = (float *)alloc->alloc(alloc->ctx, result.dimensions * sizeof(float));
    if (!out->values) {
        sc_embedding_provider_free(alloc, &result);
        return SC_ERR_OUT_OF_MEMORY;
    }
    memcpy(out->values, result.values, result.dimensions * sizeof(float));
    out->dim = result.dimensions;
    sc_embedding_provider_free(alloc, &result);
    return SC_OK;
}

static sc_error_t embed_batch_impl(void *ctx, sc_allocator_t *alloc, const char **texts,
                                   const size_t *text_lens, size_t count, sc_embedding_t *out) {
    if (count > 0 && (!texts || !text_lens || !out))
        return SC_ERR_INVALID_ARGUMENT;
    for (size_t i = 0; i < count; i++) {
        sc_error_t err = embed_impl(ctx, alloc, texts[i], text_lens[i], &out[i]);
        if (err != SC_OK) {
            for (size_t j = 0; j < i; j++)
                sc_embedding_free(alloc, &out[j]);
            return err;
        }
    }
    return SC_OK;
}

static size_t dimensions_impl(void *ctx) {
    gemini_adapter_ctx_t *adapter = (gemini_adapter_ctx_t *)ctx;
    if (!adapter || !adapter->provider.vtable)
        return 0;
    return adapter->provider.vtable->dimensions(adapter->provider.ctx);
}

static void deinit_impl(void *ctx, sc_allocator_t *alloc) {
    if (!ctx || !alloc)
        return;
    gemini_adapter_ctx_t *adapter = (gemini_adapter_ctx_t *)ctx;
    if (adapter->provider.vtable && adapter->provider.ctx)
        adapter->provider.vtable->deinit(adapter->provider.ctx, alloc);
    alloc->free(alloc->ctx, ctx, sizeof(gemini_adapter_ctx_t));
}

static const sc_embedder_vtable_t gemini_adapter_vtable = {
    .embed = embed_impl,
    .embed_batch = embed_batch_impl,
    .dimensions = dimensions_impl,
    .deinit = deinit_impl,
};

sc_embedder_t sc_embedder_gemini_adapter_create(sc_allocator_t *alloc,
                                                sc_embedding_provider_t provider) {
    sc_embedder_t emb = {.ctx = NULL, .vtable = &gemini_adapter_vtable};
    if (!alloc || !provider.vtable)
        return emb;

    gemini_adapter_ctx_t *ctx =
        (gemini_adapter_ctx_t *)alloc->alloc(alloc->ctx, sizeof(gemini_adapter_ctx_t));
    if (!ctx) {
        if (provider.vtable && provider.vtable->deinit)
            provider.vtable->deinit(provider.ctx, alloc);
        return emb;
    }
    ctx->provider = provider;

    emb.ctx = ctx;
    return emb;
}
