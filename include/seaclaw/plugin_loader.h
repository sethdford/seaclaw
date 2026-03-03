#ifndef SC_PLUGIN_LOADER_H
#define SC_PLUGIN_LOADER_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/plugin.h"
#include <stddef.h>

typedef struct sc_plugin_handle sc_plugin_handle_t;

sc_error_t sc_plugin_load(sc_allocator_t *alloc, const char *path, sc_plugin_host_t *host,
                         sc_plugin_info_t *info_out, sc_plugin_handle_t **out_handle);

void sc_plugin_unload(sc_plugin_handle_t *handle);

void sc_plugin_unload_all(void);

#endif /* SC_PLUGIN_LOADER_H */
