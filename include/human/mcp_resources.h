#ifndef HU_MCP_RESOURCES_H
#define HU_MCP_RESOURCES_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * MCP Resources + Prompts: full MCP spec beyond just Tools.
 * Resources: read-only data sources (files, DB rows, API responses).
 * Prompts: reusable prompt templates with parameter substitution.
 */

typedef struct hu_mcp_resource {
    char uri[256];
    char name[128];
    char description[256];
    char mime_type[64];
} hu_mcp_resource_t;

typedef struct hu_mcp_resource_content {
    char uri[256];
    char mime_type[64];
    char *text; /* heap-allocated */
    size_t text_len;
} hu_mcp_resource_content_t;

typedef struct hu_mcp_resource_template {
    char uri_template[256]; /* e.g. "file:///{path}" */
    char name[128];
    char description[256];
    char mime_type[64];
} hu_mcp_resource_template_t;

#define HU_MCP_MAX_RESOURCES 128

typedef struct hu_mcp_resource_registry {
    hu_mcp_resource_t resources[HU_MCP_MAX_RESOURCES];
    size_t resource_count;
    hu_mcp_resource_template_t templates[HU_MCP_MAX_RESOURCES];
    size_t template_count;
} hu_mcp_resource_registry_t;

void hu_mcp_resource_registry_init(hu_mcp_resource_registry_t *reg);

hu_error_t hu_mcp_resource_register(hu_mcp_resource_registry_t *reg, const char *uri,
                                    const char *name, const char *description,
                                    const char *mime_type);

hu_error_t hu_mcp_resource_template_register(hu_mcp_resource_registry_t *reg,
                                             const char *uri_template, const char *name,
                                             const char *description, const char *mime_type);

hu_error_t hu_mcp_resource_list_json(hu_allocator_t *alloc, const hu_mcp_resource_registry_t *reg,
                                     char **out_json, size_t *out_len);

/* MCP Prompts */

typedef struct hu_mcp_prompt_arg {
    char name[64];
    char description[256];
    bool required;
} hu_mcp_prompt_arg_t;

#define HU_MCP_MAX_PROMPT_ARGS 16

typedef struct hu_mcp_prompt {
    char name[128];
    char description[256];
    hu_mcp_prompt_arg_t arguments[HU_MCP_MAX_PROMPT_ARGS];
    size_t argument_count;
    char template_text[4096];
    size_t template_text_len;
} hu_mcp_prompt_t;

#define HU_MCP_MAX_PROMPTS 64

typedef struct hu_mcp_prompt_registry {
    hu_mcp_prompt_t prompts[HU_MCP_MAX_PROMPTS];
    size_t prompt_count;
} hu_mcp_prompt_registry_t;

void hu_mcp_prompt_registry_init(hu_mcp_prompt_registry_t *reg);

hu_error_t hu_mcp_prompt_register(hu_mcp_prompt_registry_t *reg, const hu_mcp_prompt_t *prompt);

hu_error_t hu_mcp_prompt_render(hu_allocator_t *alloc, const hu_mcp_prompt_t *prompt,
                                const char *const *arg_names, const char *const *arg_values,
                                size_t arg_count, char **out_text, size_t *out_len);

hu_error_t hu_mcp_prompt_list_json(hu_allocator_t *alloc, const hu_mcp_prompt_registry_t *reg,
                                   char **out_json, size_t *out_len);

#endif /* HU_MCP_RESOURCES_H */
