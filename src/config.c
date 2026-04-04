#include "human/config.h"
#include "human/core/arena.h"
#include "human/core/error.h"
#include "human/core/string.h"
#include <string.h>

#include "config_internal.h"

void hu_config_deinit(hu_config_t *cfg) {
    if (!cfg)
        return;
    hu_allocator_t *a = &cfg->allocator;
    for (size_t i = 0; i < HU_ENSEMBLE_CONFIG_PROVIDER_NAMES_MAX; i++) {
        if (cfg->ensemble.providers[i]) {
            a->free(a->ctx, cfg->ensemble.providers[i],
                    strlen(cfg->ensemble.providers[i]) + 1);
            cfg->ensemble.providers[i] = NULL;
        }
    }
    cfg->ensemble.providers_len = 0;
    if (cfg->ensemble.strategy) {
        a->free(a->ctx, cfg->ensemble.strategy, strlen(cfg->ensemble.strategy) + 1);
        cfg->ensemble.strategy = NULL;
    }
    if (cfg->arena) {
        /* Arena holds most config strings (e.g. cfg->voice.* including mode, realtime_model,
         * realtime_voice); bulk-freed here. */
        hu_arena_destroy(cfg->arena);
        cfg->arena = NULL;
    }
    memset(cfg, 0, sizeof(*cfg));
}

/* parse_* and hu_config_parse_json moved to config_parse.c */
/* hu_config_load, set_defaults, sync_*, load_json_file, hu_config_apply_env_* moved to
 * config_merge.c */
/* hu_config_save moved to config_serialize.c */
/* hu_config_get_*, hu_config_validate, hu_config_provider_requires_api_key moved to
 * config_getters.c */
