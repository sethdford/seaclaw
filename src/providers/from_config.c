/* Create provider from config (handles composite providers like "reliable"). */
#include "seaclaw/config.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/providers/factory.h"
#include "seaclaw/providers/reliable.h"
#include <string.h>

sc_error_t sc_provider_create_from_config(sc_allocator_t *alloc, const sc_config_t *cfg,
                                          const char *name, size_t name_len, sc_provider_t *out) {
    if (!alloc || !cfg || !name || name_len == 0 || !out)
        return SC_ERR_INVALID_ARGUMENT;

    if (name_len == 8 && memcmp(name, "reliable", 8) == 0) {
        const char *primary_name =
            cfg->reliability.primary_provider && cfg->reliability.primary_provider[0]
                ? cfg->reliability.primary_provider
                : "openai";
        size_t primary_len = strlen(primary_name);
        const char *api_key = sc_config_get_provider_key(cfg, primary_name);
        size_t api_key_len = api_key ? strlen(api_key) : 0;
        const char *base_url = sc_config_get_provider_base_url(cfg, primary_name);
        size_t base_url_len = base_url ? strlen(base_url) : 0;
        if (!base_url || base_url_len == 0) {
            base_url = sc_compatible_provider_url(primary_name);
            base_url_len = base_url ? strlen(base_url) : 0;
        }

        sc_provider_t primary;
        sc_error_t err = sc_provider_create(alloc, primary_name, primary_len, api_key, api_key_len,
                                            base_url, base_url_len, &primary);
        if (err != SC_OK)
            return err;

        sc_provider_t fallback = {0};
        if (cfg->reliability.fallback_providers_len > 0 &&
            cfg->reliability.fallback_providers[0] && cfg->reliability.fallback_providers[0][0]) {
            const char *fb_name = cfg->reliability.fallback_providers[0];
            size_t fb_len = strlen(fb_name);
            const char *fb_key = sc_config_get_provider_key(cfg, fb_name);
            size_t fb_key_len = fb_key ? strlen(fb_key) : 0;
            const char *fb_url = sc_config_get_provider_base_url(cfg, fb_name);
            size_t fb_url_len = fb_url ? strlen(fb_url) : 0;
            if (!fb_url || fb_url_len == 0) {
                fb_url = sc_compatible_provider_url(fb_name);
                fb_url_len = fb_url ? strlen(fb_url) : 0;
            }
            err = sc_provider_create(alloc, fb_name, fb_len, fb_key, fb_key_len, fb_url, fb_url_len,
                                     &fallback);
            if (err != SC_OK) {
                if (primary.vtable && primary.vtable->deinit)
                    primary.vtable->deinit(primary.ctx, alloc);
                return err;
            }
        }

        sc_reliable_config_t rcfg = {
            .primary = primary,
            .fallback = fallback,
            .max_retries = cfg->reliability.provider_retries > 0 ? (int)cfg->reliability.provider_retries : 3,
            .base_delay_ms = cfg->reliability.provider_backoff_ms > 0 ? (int)cfg->reliability.provider_backoff_ms : 1000,
            .max_delay_ms = 30000,
            .failure_threshold = 5,
            .recovery_timeout_seconds = 60,
        };

        err = sc_reliable_provider_create(alloc, &rcfg, out);
        if (err != SC_OK) {
            if (primary.vtable && primary.vtable->deinit)
                primary.vtable->deinit(primary.ctx, alloc);
            if (fallback.vtable && fallback.vtable->deinit)
                fallback.vtable->deinit(fallback.ctx, alloc);
            return err;
        }
        return SC_OK;
    }

    return SC_ERR_NOT_SUPPORTED;
}
