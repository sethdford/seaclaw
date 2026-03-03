#ifndef SC_MCP_REGISTRY_H
#define SC_MCP_REGISTRY_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stdbool.h>
#include <stddef.h>

#define SC_MCP_REGISTRY_ENTRIES_MAX 16

typedef struct sc_mcp_registry_entry {
    char name[64];
    char command[256];
    char args[512];
    bool running;
    int pid;
} sc_mcp_registry_entry_t;

typedef struct sc_mcp_registry sc_mcp_registry_t;

sc_mcp_registry_t *sc_mcp_registry_create(sc_allocator_t *alloc);
void sc_mcp_registry_destroy(sc_mcp_registry_t *reg);

sc_error_t sc_mcp_registry_add(sc_mcp_registry_t *reg, const char *name, const char *command,
                               const char *args);
sc_error_t sc_mcp_registry_remove(sc_mcp_registry_t *reg, const char *name);
sc_error_t sc_mcp_registry_list(sc_mcp_registry_t *reg, sc_mcp_registry_entry_t *out, int max,
                                int *count);
sc_error_t sc_mcp_registry_start(sc_mcp_registry_t *reg, const char *name);
sc_error_t sc_mcp_registry_stop(sc_mcp_registry_t *reg, const char *name);

#endif /* SC_MCP_REGISTRY_H */
