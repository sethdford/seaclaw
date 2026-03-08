#include "config_internal.h"
#include "config_parse_internal.h"
#include "seaclaw/config.h"
#include "seaclaw/core/string.h"
#include <string.h>

sc_error_t parse_providers(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *arr) {
    if (!arr || arr->type != SC_JSON_ARRAY)
        return SC_OK;
    size_t cap = arr->data.array.len;
    if (cap == 0)
        return SC_OK;

    sc_provider_entry_t *providers =
        (sc_provider_entry_t *)a->alloc(a->ctx, cap * sizeof(sc_provider_entry_t));
    if (!providers)
        return SC_ERR_OUT_OF_MEMORY;
    memset(providers, 0, cap * sizeof(sc_provider_entry_t));

    size_t n = 0;
    for (size_t i = 0; i < arr->data.array.len; i++) {
        const sc_json_value_t *item = arr->data.array.items[i];
        if (!item || item->type != SC_JSON_OBJECT)
            continue;

        const char *name = sc_json_get_string(item, "name");
        if (!name)
            continue;

        providers[n].name = sc_strdup(a, name);
        const char *api_key = sc_json_get_string(item, "api_key");
        if (api_key)
            providers[n].api_key = sc_strdup(a, api_key);
        const char *base_url = sc_json_get_string(item, "base_url");
        if (base_url)
            providers[n].base_url = sc_strdup(a, base_url);
        providers[n].native_tools = sc_json_get_bool(item, "native_tools", true);
        providers[n].ws_streaming = sc_json_get_bool(item, "ws_streaming", false);

        if (providers[n].name) {
            n++;
        } else {
            /* OOM on name; free partial allocations before next iteration overwrites */
            if (providers[n].api_key) {
                a->free(a->ctx, providers[n].api_key, strlen(providers[n].api_key) + 1);
                providers[n].api_key = NULL;
            }
            if (providers[n].base_url) {
                a->free(a->ctx, providers[n].base_url, strlen(providers[n].base_url) + 1);
                providers[n].base_url = NULL;
            }
        }
    }
    cfg->providers = providers;
    cfg->providers_len = n;
    return SC_OK;
}

sc_error_t parse_router(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    const char *fast = sc_json_get_string(obj, "fast");
    if (fast) {
        if (cfg->router.fast)
            a->free(a->ctx, cfg->router.fast, strlen(cfg->router.fast) + 1);
        cfg->router.fast = sc_strdup(a, fast);
    }
    const char *standard = sc_json_get_string(obj, "standard");
    if (standard) {
        if (cfg->router.standard)
            a->free(a->ctx, cfg->router.standard, strlen(cfg->router.standard) + 1);
        cfg->router.standard = sc_strdup(a, standard);
    }
    const char *powerful = sc_json_get_string(obj, "powerful");
    if (powerful) {
        if (cfg->router.powerful)
            a->free(a->ctx, cfg->router.powerful, strlen(cfg->router.powerful) + 1);
        cfg->router.powerful = sc_strdup(a, powerful);
    }
    double cl = sc_json_get_number(obj, "complexity_low", (double)cfg->router.complexity_low);
    if (cl >= 0 && cl <= 10000)
        cfg->router.complexity_low = (int)cl;
    double ch = sc_json_get_number(obj, "complexity_high", (double)cfg->router.complexity_high);
    if (ch >= 0 && ch <= 100000)
        cfg->router.complexity_high = (int)ch;
    return SC_OK;
}
