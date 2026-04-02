/*
 * Fuzz harness for MCP manager tool name parsing, building, and server lookup.
 * Must not crash, overflow, or leak on any input.
 *
 * Exercises: hu_mcp_tool_is_mcp, hu_mcp_tool_parse_name,
 *            hu_mcp_tool_build_name, hu_mcp_manager_create,
 *            hu_mcp_manager_find_server, hu_mcp_manager_call_tool.
 */
#include "human/mcp_manager.h"
#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 8192 || size < 2)
        return 0;

    hu_allocator_t alloc = hu_system_allocator();

    /* NUL-terminate the input for string functions */
    char *input = (char *)alloc.alloc(alloc.ctx, size + 1);
    if (!input)
        return 0;
    memcpy(input, data, size);
    input[size] = '\0';

    /* ── Fuzz hu_mcp_tool_is_mcp ── */
    (void)hu_mcp_tool_is_mcp(input);

    /* ── Fuzz hu_mcp_tool_parse_name ── */
    {
        const char *server = NULL;
        size_t server_len = 0;
        const char *tool = NULL;
        size_t tool_len = 0;
        (void)hu_mcp_tool_parse_name(input, &server, &server_len, &tool, &tool_len);
    }

    /* ── Fuzz hu_mcp_tool_build_name with split input ── */
    {
        /* Split fuzz data at first byte as offset for the server/tool boundary */
        size_t split = data[0] % size;
        if (split == 0) split = 1;
        if (split >= size) split = size - 1;

        char *server_part = (char *)alloc.alloc(alloc.ctx, split + 1);
        char *tool_part = (char *)alloc.alloc(alloc.ctx, (size - split) + 1);
        if (server_part && tool_part) {
            memcpy(server_part, data + 1, split);
            server_part[split] = '\0';
            memcpy(tool_part, data + 1 + split, size - 1 - split);
            tool_part[size - 1 - split] = '\0';

            char *built = NULL;
            size_t built_len = 0;
            hu_error_t err = hu_mcp_tool_build_name(&alloc, server_part, tool_part,
                                                    &built, &built_len);
            if (err == HU_OK && built) {
                /* Round-trip: parse the built name */
                const char *ps = NULL, *pt = NULL;
                size_t psl = 0, ptl = 0;
                (void)hu_mcp_tool_parse_name(built, &ps, &psl, &pt, &ptl);
                alloc.free(alloc.ctx, built, built_len + 1);
            }

            alloc.free(alloc.ctx, server_part, split + 1);
            alloc.free(alloc.ctx, tool_part, (size - split) + 1);
        } else {
            if (server_part) alloc.free(alloc.ctx, server_part, split + 1);
            if (tool_part) alloc.free(alloc.ctx, tool_part, (size - split) + 1);
        }
    }

    /* ── Fuzz hu_mcp_manager_create with synthetic config ── */
    {
        /* Use first few bytes to construct 1-2 server entries with fuzz names */
        size_t name_len = data[0] % 64;
        if (name_len == 0) name_len = 1;
        if (name_len > size - 1) name_len = size - 1;

        char *srv_name = (char *)alloc.alloc(alloc.ctx, name_len + 1);
        if (srv_name) {
            memcpy(srv_name, data + 1, name_len);
            srv_name[name_len] = '\0';

            hu_mcp_server_entry_t entry;
            memset(&entry, 0, sizeof(entry));
            entry.name = srv_name;
            entry.command = "/nonexistent/binary";
            entry.transport_type = "stdio";
            entry.auto_connect = false;
            entry.timeout_ms = 1000;

            hu_mcp_manager_t *mgr = NULL;
            hu_error_t err = hu_mcp_manager_create(&alloc, &entry, 1, &mgr);
            if (err == HU_OK && mgr) {
                /* Try to find server by fuzz name */
                size_t idx = 0;
                (void)hu_mcp_manager_find_server(mgr, input, &idx);
                (void)hu_mcp_manager_server_count(mgr);

                /* Try calling a tool on a disconnected server (should fail gracefully) */
                char *result = NULL;
                size_t result_len = 0;
                (void)hu_mcp_manager_call_tool(mgr, &alloc, srv_name, "test_tool",
                                               "{}", &result, &result_len);
                if (result)
                    alloc.free(alloc.ctx, result, result_len + 1);

                hu_mcp_manager_destroy(mgr);
            }

            alloc.free(alloc.ctx, srv_name, name_len + 1);
        }
    }

    alloc.free(alloc.ctx, input, size + 1);
    return 0;
}
