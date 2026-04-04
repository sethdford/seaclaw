/* Create provider from config (handles composite providers like "reliable", "router"). */
#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/providers/ensemble.h"
#include "human/providers/factory.h"
#include "human/providers/reliable.h"
#include "human/providers/router.h"
#include <stdint.h>
#include <string.h>

static hu_error_t create_provider_from_name(hu_allocator_t *alloc, const hu_config_t *cfg,
                                            const char *prov_name, hu_provider_t *out) {
    if (!prov_name || !prov_name[0])
        return HU_ERR_INVALID_ARGUMENT;
    size_t len = strlen(prov_name);
    const char *api_key = hu_config_get_provider_key(cfg, prov_name);
    size_t api_key_len = api_key ? strlen(api_key) : 0;
    const char *base_url = hu_config_get_provider_base_url(cfg, prov_name);
    size_t base_url_len = base_url ? strlen(base_url) : 0;
    if (!base_url || base_url_len == 0) {
        base_url = hu_compatible_provider_url(prov_name);
        base_url_len = base_url ? strlen(base_url) : 0;
    }
    return hu_provider_create(alloc, prov_name, len, api_key, api_key_len, base_url, base_url_len,
                              out);
}

hu_error_t hu_provider_create_from_config(hu_allocator_t *alloc, const hu_config_t *cfg,
                                          const char *name, size_t name_len, hu_provider_t *out) {
    if (!alloc || !cfg || !name || name_len == 0 || !out)
        return HU_ERR_INVALID_ARGUMENT;

    if (name_len == 6 && memcmp(name, "router", 6) == 0) {
        const char *standard_name =
            cfg->router.standard && cfg->router.standard[0] ? cfg->router.standard : "openai";
        hu_provider_t standard = {0};
        hu_error_t err = create_provider_from_name(alloc, cfg, standard_name, &standard);
        if (err != HU_OK)
            return err;

        hu_provider_t fast = {0};
        hu_provider_t powerful = {0};
        bool has_fast = (cfg->router.fast && cfg->router.fast[0]);
        bool has_powerful = (cfg->router.powerful && cfg->router.powerful[0]);

        if (has_fast) {
            err = create_provider_from_name(alloc, cfg, cfg->router.fast, &fast);
            if (err != HU_OK) {
                if (standard.vtable && standard.vtable->deinit)
                    standard.vtable->deinit(standard.ctx, alloc);
                return err;
            }
        }
        if (has_powerful) {
            err = create_provider_from_name(alloc, cfg, cfg->router.powerful, &powerful);
            if (err != HU_OK) {
                if (standard.vtable && standard.vtable->deinit)
                    standard.vtable->deinit(standard.ctx, alloc);
                if (has_fast && fast.vtable && fast.vtable->deinit)
                    fast.vtable->deinit(fast.ctx, alloc);
                return err;
            }
        }

        hu_multi_model_router_config_t rcfg = {
            .fast = fast,
            .standard = standard,
            .powerful = powerful,
            .complexity_threshold_low =
                cfg->router.complexity_low > 0 ? cfg->router.complexity_low : 50,
            .complexity_threshold_high =
                cfg->router.complexity_high > 0 ? cfg->router.complexity_high : 500,
        };
        err = hu_multi_model_router_create(alloc, &rcfg, out);
        if (err != HU_OK) {
            if (standard.vtable && standard.vtable->deinit)
                standard.vtable->deinit(standard.ctx, alloc);
            if (has_fast && fast.vtable && fast.vtable->deinit)
                fast.vtable->deinit(fast.ctx, alloc);
            if (has_powerful && powerful.vtable && powerful.vtable->deinit)
                powerful.vtable->deinit(powerful.ctx, alloc);
            return err;
        }
        return HU_OK;
    }

    if (name_len == 8 && memcmp(name, "reliable", 8) == 0) {
        const char *primary_name =
            cfg->reliability.primary_provider && cfg->reliability.primary_provider[0]
                ? cfg->reliability.primary_provider
                : "openai";
        size_t primary_len = strlen(primary_name);
        const char *api_key = hu_config_get_provider_key(cfg, primary_name);
        size_t api_key_len = api_key ? strlen(api_key) : 0;
        const char *base_url = hu_config_get_provider_base_url(cfg, primary_name);
        size_t base_url_len = base_url ? strlen(base_url) : 0;
        if (!base_url || base_url_len == 0) {
            base_url = hu_compatible_provider_url(primary_name);
            base_url_len = base_url ? strlen(base_url) : 0;
        }

        hu_provider_t primary;
        hu_error_t err = hu_provider_create(alloc, primary_name, primary_len, api_key, api_key_len,
                                            base_url, base_url_len, &primary);
        if (err != HU_OK)
            return err;

        hu_reliable_provider_entry_t extras[HU_MAX_FALLBACK_PROVIDERS];
        hu_provider_t fallback_provs[HU_MAX_FALLBACK_PROVIDERS];
        size_t extras_count = 0;
        memset(fallback_provs, 0, sizeof(fallback_provs));

        size_t max_fb = cfg->reliability.fallback_providers_len;
        if (max_fb > HU_MAX_FALLBACK_PROVIDERS)
            max_fb = HU_MAX_FALLBACK_PROVIDERS;
        for (size_t fi = 0; fi < max_fb; fi++) {
            const char *fb_name = cfg->reliability.fallback_providers[fi];
            if (!fb_name || !fb_name[0])
                continue;
            if (strcmp(fb_name, primary_name) == 0)
                continue;
            size_t fb_len = strlen(fb_name);
            const char *fb_key = hu_config_get_provider_key(cfg, fb_name);
            size_t fb_key_len = fb_key ? strlen(fb_key) : 0;
            const char *fb_url = hu_config_get_provider_base_url(cfg, fb_name);
            size_t fb_url_len = fb_url ? strlen(fb_url) : 0;
            if (!fb_url || fb_url_len == 0) {
                fb_url = hu_compatible_provider_url(fb_name);
                fb_url_len = fb_url ? strlen(fb_url) : 0;
            }
            err = hu_provider_create(alloc, fb_name, fb_len, fb_key, fb_key_len, fb_url, fb_url_len,
                                     &fallback_provs[extras_count]);
            if (err != HU_OK) {
                for (size_t j = 0; j < extras_count; j++) {
                    if (fallback_provs[j].vtable && fallback_provs[j].vtable->deinit)
                        fallback_provs[j].vtable->deinit(fallback_provs[j].ctx, alloc);
                }
                if (primary.vtable && primary.vtable->deinit)
                    primary.vtable->deinit(primary.ctx, alloc);
                return err;
            }
            extras[extras_count].name = fb_name;
            extras[extras_count].name_len = fb_len;
            extras[extras_count].provider = fallback_provs[extras_count];
            extras_count++;
        }

        hu_reliable_fallback_model_t
            fb_buf[HU_MAX_RELIABILITY_MODEL_FALLBACK_ROWS][HU_RELIABLE_MAX_FALLBACK_MODEL_NAMES];
        hu_reliable_model_fallback_entry_t model_entries[HU_MAX_RELIABILITY_MODEL_FALLBACK_ROWS];
        memset(fb_buf, 0, sizeof(fb_buf));
        memset(model_entries, 0, sizeof(model_entries));

        size_t n_mf = cfg->reliability.model_fallbacks_len;
        if (n_mf > HU_MAX_RELIABILITY_MODEL_FALLBACK_ROWS)
            n_mf = HU_MAX_RELIABILITY_MODEL_FALLBACK_ROWS;
        size_t model_fb_count = 0;
        for (size_t i = 0; i < n_mf; i++) {
            const hu_reliability_model_fallback_t *row = &cfg->reliability.model_fallbacks[i];
            if (!row->model || !row->model[0])
                continue;
            size_t flen = row->fallback_models_len;
            if (flen > HU_RELIABLE_MAX_FALLBACK_MODEL_NAMES)
                flen = HU_RELIABLE_MAX_FALLBACK_MODEL_NAMES;
            size_t use_flen = 0;
            for (size_t j = 0; j < flen; j++) {
                const char *fm = row->fallback_models[j];
                if (!fm || !fm[0])
                    continue;
                fb_buf[model_fb_count][use_flen].model = fm;
                fb_buf[model_fb_count][use_flen].model_len = strlen(fm);
                use_flen++;
            }
            model_entries[model_fb_count].model = row->model;
            model_entries[model_fb_count].model_len = strlen(row->model);
            model_entries[model_fb_count].fallbacks = use_flen > 0 ? fb_buf[model_fb_count] : NULL;
            model_entries[model_fb_count].fallbacks_count = use_flen;
            model_fb_count++;
        }

        uint32_t max_retries =
            cfg->reliability.provider_retries > 0 ? cfg->reliability.provider_retries : 3;
        uint64_t backoff_ms =
            cfg->reliability.provider_backoff_ms > 0 ? cfg->reliability.provider_backoff_ms : 1000;

        hu_reliable_extended_opts_t ext = {
            .max_backoff_ms = 30000,
            .failure_threshold = cfg->reliability.circuit_breaker_enabled ? 5 : 0,
            .recovery_timeout_seconds = 60,
            .streaming_retries = cfg->reliability.streaming_retries,
        };

        err = hu_reliable_create_ex(alloc, primary, max_retries, backoff_ms,
                                    extras_count > 0 ? extras : NULL, extras_count,
                                    model_fb_count > 0 ? model_entries : NULL, model_fb_count, &ext,
                                    out);
        if (err != HU_OK) {
            for (size_t j = 0; j < extras_count; j++) {
                if (fallback_provs[j].vtable && fallback_provs[j].vtable->deinit)
                    fallback_provs[j].vtable->deinit(fallback_provs[j].ctx, alloc);
            }
            if (primary.vtable && primary.vtable->deinit)
                primary.vtable->deinit(primary.ctx, alloc);
            return err;
        }
        return HU_OK;
    }

    if (name_len == 8 && memcmp(name, "ensemble", 8) == 0) {
        const char *names_buf[HU_ENSEMBLE_MAX_PROVIDERS];
        size_t name_count = 0;

        if (cfg->ensemble.providers_len > 0) {
            for (size_t i = 0; i < cfg->ensemble.providers_len && name_count < HU_ENSEMBLE_MAX_PROVIDERS;
                 i++) {
                const char *pname = cfg->ensemble.providers[i];
                if (pname && pname[0])
                    names_buf[name_count++] = pname;
            }
        } else {
            if (cfg->default_provider && cfg->default_provider[0])
                names_buf[name_count++] = cfg->default_provider;
            if (cfg->reliability.fallback_providers_len > 0 &&
                cfg->reliability.fallback_providers[0] &&
                cfg->reliability.fallback_providers[0][0] &&
                name_count < HU_ENSEMBLE_MAX_PROVIDERS)
                names_buf[name_count++] = cfg->reliability.fallback_providers[0];
        }

        hu_ensemble_spec_t ecfg = {0};
        hu_error_t err = HU_OK;
        for (size_t i = 0; i < name_count; i++) {
            err = create_provider_from_name(alloc, cfg, names_buf[i],
                                            &ecfg.providers[ecfg.provider_count]);
            if (err == HU_OK)
                ecfg.provider_count++;
        }

        if (ecfg.provider_count == 0)
            return HU_ERR_INVALID_ARGUMENT;

        const char *strat = cfg->ensemble.strategy;
        if (strat && strcmp(strat, "best_for_task") == 0)
            ecfg.strategy = HU_ENSEMBLE_BEST_FOR_TASK;
        else if (strat && strcmp(strat, "consensus") == 0)
            ecfg.strategy = HU_ENSEMBLE_CONSENSUS;
        else
            ecfg.strategy = HU_ENSEMBLE_ROUND_ROBIN;

        err = hu_ensemble_create(alloc, &ecfg, out);
        if (err != HU_OK) {
            for (size_t i = 0; i < ecfg.provider_count; i++) {
                if (ecfg.providers[i].vtable && ecfg.providers[i].vtable->deinit)
                    ecfg.providers[i].vtable->deinit(ecfg.providers[i].ctx, alloc);
            }
        }
        return err;
    }

    /* Plain providers (openai, gemini, groq, …): delegate to factory via config keys/URLs. */
    {
        char nbuf[160];
        if (name_len >= sizeof(nbuf))
            return HU_ERR_INVALID_ARGUMENT;
        memcpy(nbuf, name, name_len);
        nbuf[name_len] = '\0';
        return create_provider_from_name(alloc, cfg, nbuf, out);
    }
}
