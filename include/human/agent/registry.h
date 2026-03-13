#ifndef HU_AGENT_REGISTRY_H
#define HU_AGENT_REGISTRY_H

#include "human/config_types.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Agent Registry — discovers and manages named agent definitions from JSON
 * files on disk (~/.human/agents/). Provides lookup by name and
 * capability matching for orchestrator-driven delegation.
 */

#define HU_AGENT_REGISTRY_MAX 32

typedef struct hu_agent_registry {
    hu_allocator_t *alloc;
    hu_named_agent_config_t agents[HU_AGENT_REGISTRY_MAX];
    size_t count;
    char discover_dir[512];
} hu_agent_registry_t;

hu_error_t hu_agent_registry_create(hu_allocator_t *alloc, hu_agent_registry_t *out);
void hu_agent_registry_destroy(hu_agent_registry_t *reg);

/* Load all *.json from a directory (e.g. ~/.human/agents/). */
hu_error_t hu_agent_registry_discover(hu_agent_registry_t *reg, const char *dir_path);

/* Register a single agent definition. Copies all strings. */
hu_error_t hu_agent_registry_register(hu_agent_registry_t *reg,
                                      const hu_named_agent_config_t *cfg);

/* Lookup by name. Returns NULL if not found. */
const hu_named_agent_config_t *hu_agent_registry_get(const hu_agent_registry_t *reg,
                                                     const char *name);

/* Find agents whose capabilities contain the given tag. Writes up to max_out
 * pointers into out[] and sets *count. */
hu_error_t hu_agent_registry_find_by_capability(const hu_agent_registry_t *reg,
                                                const char *capability,
                                                const hu_named_agent_config_t **out,
                                                size_t max_out, size_t *count);

/* Get the default agent (first one with is_default=true, or the first entry). */
const hu_named_agent_config_t *hu_agent_registry_get_default(const hu_agent_registry_t *reg);

/* Reload from the previously-discovered directory. */
hu_error_t hu_agent_registry_reload(hu_agent_registry_t *reg);

/* Parse a single agent config from JSON. Caller must free via hu_named_agent_config_free. */
hu_error_t hu_agent_registry_parse_json(hu_allocator_t *alloc, const char *json,
                                        size_t json_len, hu_named_agent_config_t *out);

#endif /* HU_AGENT_REGISTRY_H */
