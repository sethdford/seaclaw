#ifndef HU_MCP_MANAGER_H
#define HU_MCP_MANAGER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tool.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Forward declarations */
struct hu_config;
struct hu_mcp_server_entry;

/* ──────────────────────────────────────────────────────────────────────────
 * MCP Manager — orchestrates multiple MCP servers, discovers tools,
 * and wraps them as hu_tool_vtable_t instances.
 *
 * Tool naming convention: mcp__<server_name>__<tool_name>
 * ────────────────────────────────────────────────────────────────────────── */

#define HU_MCP_MANAGER_MAX_SERVERS     16
#define HU_MCP_MANAGER_MAX_RESPONSE    (100u * 1024u * 1024u) /* 100 MB */
#define HU_MCP_MANAGER_DEFAULT_TIMEOUT 30000u                 /* 30 seconds */

typedef struct hu_mcp_manager hu_mcp_manager_t;

/* ── Server info (read-only view) ─────────────────────────────────────── */

typedef struct hu_mcp_server_info {
    const char *name;
    const char *command;
    bool connected;
    size_t tool_count;
    uint32_t timeout_ms;
} hu_mcp_server_info_t;

/* ── Lifecycle ────────────────────────────────────────────────────────── */

/**
 * Create an MCP manager from config entries.
 *
 * @param alloc   Allocator for all internal allocations.
 * @param entries Array of server config entries (from hu_config_t.mcp_servers).
 * @param count   Number of entries.
 * @param out     Receives the created manager. Caller must call hu_mcp_manager_destroy().
 * @return HU_OK on success.
 */
hu_error_t hu_mcp_manager_create(hu_allocator_t *alloc,
                                 const struct hu_mcp_server_entry *entries, size_t count,
                                 hu_mcp_manager_t **out);

/**
 * Destroy the manager and all owned servers.
 * NULL-safe.
 */
void hu_mcp_manager_destroy(hu_mcp_manager_t *mgr);

/* ── Connection ───────────────────────────────────────────────────────── */

/**
 * Connect all servers that have auto_connect enabled.
 * Servers that fail to connect are silently skipped (logged via stderr).
 */
hu_error_t hu_mcp_manager_connect_auto(hu_mcp_manager_t *mgr);

/**
 * Connect a specific server by name.
 */
hu_error_t hu_mcp_manager_connect_server(hu_mcp_manager_t *mgr, const char *server_name);

/* ── Tool Discovery ───────────────────────────────────────────────────── */

/**
 * Discover tools from all connected servers and wrap them as hu_tool_t.
 *
 * Tool names follow the mcp__<server>__<tool> convention.
 * Caller must free the returned array with hu_mcp_manager_free_tools().
 *
 * @param mgr       The manager.
 * @param alloc     Allocator for tool wrappers and the array.
 * @param out_tools Receives an array of hu_tool_t.
 * @param out_count Receives the number of tools discovered.
 * @return HU_OK on success.
 */
hu_error_t hu_mcp_manager_load_tools(hu_mcp_manager_t *mgr, hu_allocator_t *alloc,
                                     hu_tool_t **out_tools, size_t *out_count);

/**
 * Free tools returned by hu_mcp_manager_load_tools().
 */
void hu_mcp_manager_free_tools(hu_allocator_t *alloc, hu_tool_t *tools, size_t count);

/* ── Direct Tool Call ─────────────────────────────────────────────────── */

/**
 * Call a tool on a specific server by name.
 *
 * @param mgr          The manager.
 * @param alloc        Allocator for the result.
 * @param server_name  Name of the server.
 * @param tool_name    Name of the tool (without server prefix).
 * @param args_json    JSON object string with arguments.
 * @param out_result   Receives the result string (caller frees with alloc).
 * @param out_result_len Receives the length of the result.
 * @return HU_OK on success.
 */
hu_error_t hu_mcp_manager_call_tool(hu_mcp_manager_t *mgr, hu_allocator_t *alloc,
                                    const char *server_name, const char *tool_name,
                                    const char *args_json, char **out_result,
                                    size_t *out_result_len);

/* ── Queries ──────────────────────────────────────────────────────────── */

/**
 * Get the number of servers in the manager.
 */
size_t hu_mcp_manager_server_count(const hu_mcp_manager_t *mgr);

/**
 * Get info about a server by index.
 * Returns HU_ERR_NOT_FOUND if index is out of range.
 */
hu_error_t hu_mcp_manager_server_info(const hu_mcp_manager_t *mgr, size_t index,
                                      hu_mcp_server_info_t *out);

/**
 * Find a server index by name.
 * Returns HU_ERR_NOT_FOUND if no server with that name exists.
 */
hu_error_t hu_mcp_manager_find_server(const hu_mcp_manager_t *mgr, const char *name,
                                      size_t *out_index);

/* ── Tool name utilities (mcp_tool_wrapper.c) ─────────────────────────── */

/**
 * Check if a tool name follows the MCP naming convention (mcp__*).
 */
bool hu_mcp_tool_is_mcp(const char *tool_name);

/**
 * Parse an MCP tool name into server and tool components.
 * E.g., "mcp__myserver__read_file" -> server="myserver", tool="read_file".
 * Returns false if the name doesn't match the convention.
 */
bool hu_mcp_tool_parse_name(const char *prefixed_name, const char **out_server,
                            size_t *out_server_len, const char **out_tool,
                            size_t *out_tool_len);

/**
 * Build an MCP tool name from server and tool components.
 * Caller frees *out with alloc.
 */
hu_error_t hu_mcp_tool_build_name(hu_allocator_t *alloc, const char *server_name,
                                  const char *tool_name, char **out, size_t *out_len);

#endif /* HU_MCP_MANAGER_H */
