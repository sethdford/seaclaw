#ifndef HU_DECLARATIVE_TOOL_H
#define HU_DECLARATIVE_TOOL_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tool.h"
#include <stddef.h>

typedef enum hu_decl_exec_type {
    HU_DECL_EXEC_HTTP,
    HU_DECL_EXEC_SHELL,
    HU_DECL_EXEC_CHAIN,
    HU_DECL_EXEC_TRANSFORM
} hu_decl_exec_type_t;

typedef struct hu_declarative_tool_def {
    char *name;
    char *description;
    char *parameters_json;
    hu_decl_exec_type_t exec_type;
    char *exec_url;       /* HTTP: URL template */
    char *exec_method;    /* HTTP: GET/POST */
    char *exec_command;   /* SHELL: command template */
    char *exec_chain;     /* CHAIN: target tool name */
    char *exec_transform; /* TRANSFORM: jq-like expression */
} hu_declarative_tool_def_t;

/* Discover declarative tool definitions from a directory (*.json, *.yaml, *.yml).
 * Under HU_IS_TEST: returns 0 tools. Caller frees defs with hu_declarative_tool_def_free. */
hu_error_t hu_declarative_tools_discover(hu_allocator_t *alloc, const char *dir,
                                         hu_declarative_tool_def_t **out, size_t *out_count);

/* Create a hu_tool_t from a declarative definition. */
hu_error_t hu_declarative_tool_create(hu_allocator_t *alloc, const hu_declarative_tool_def_t *def,
                                      hu_tool_t *out);

/* Free a declarative tool definition. */
void hu_declarative_tool_def_free(hu_declarative_tool_def_t *def, hu_allocator_t *alloc);

#endif
