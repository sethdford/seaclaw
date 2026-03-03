#ifndef SC_MY_PROVIDER_H
#define SC_MY_PROVIDER_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/provider.h"
#include <stddef.h>

/**
 * Create a custom AI provider implementing sc_provider_t.
 *
 * Register in src/providers/factory.c:
 *   if (strcmp(name, "my_provider") == 0)
 *       return sc_my_provider_create(alloc, api_key, api_key_len, base_url, base_url_len, out);
 */
sc_error_t sc_my_provider_create(sc_allocator_t *alloc,
    const char *api_key, size_t api_key_len,
    const char *base_url, size_t base_url_len,
    sc_provider_t *out);

/** Call prov->vtable->deinit(prov->ctx, alloc) with the same allocator used at create. */
void sc_my_provider_destroy(sc_provider_t *prov, sc_allocator_t *alloc);

#endif /* SC_MY_PROVIDER_H */
