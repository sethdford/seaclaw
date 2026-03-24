#ifndef HU_PLUGIN_DISCOVERY_H
#define HU_PLUGIN_DISCOVERY_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/plugin.h"
#include "human/plugin_loader.h"
#include <stddef.h>

/*
 * Plugin auto-discovery — scans ~/.human/plugins/ for shared libraries,
 * loads them via hu_plugin_load, and registers their tools/providers/channels.
 *
 * On POSIX: scans for *.so / *.dylib files.
 * On Windows: returns HU_ERR_NOT_SUPPORTED.
 * Under HU_IS_TEST: simulates discovery without filesystem or dlopen.
 */

typedef struct hu_plugin_discovery_result {
    char *path;
    char *name;
    char *version;
    hu_error_t load_error; /* HU_OK if loaded successfully */
} hu_plugin_discovery_result_t;

/* Scan a directory for plugin shared libraries and load them.
 * If dir is NULL, defaults to ~/.human/plugins/.
 * Caller frees results with hu_plugin_discovery_results_free. */
hu_error_t hu_plugin_discover_and_load(hu_allocator_t *alloc, const char *dir,
                                       hu_plugin_host_t *host,
                                       hu_plugin_discovery_result_t **results,
                                       size_t *result_count);

void hu_plugin_discovery_results_free(hu_allocator_t *alloc, hu_plugin_discovery_result_t *results,
                                      size_t count);

/* Get the default plugin directory (~/.human/plugins/). Returns 0 if HOME unset. */
size_t hu_plugin_get_default_dir(char *out, size_t out_len);

#endif /* HU_PLUGIN_DISCOVERY_H */
