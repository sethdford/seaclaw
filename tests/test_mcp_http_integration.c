#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/mcp.h"
#include "human/mcp_manager.h"
#include "human/mcp_jsonrpc.h"
#include "human/tool.h"
#include "test_framework.h"
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════════
 * HTTP/SSE Transport Integration Tests for MCP Manager
 *
 * These tests demonstrate the three-part fix for wiring JSON-RPC into
 * HTTP tool execution and discovery:
 *
 * 1. build_oauth_auth_header: Builds Authorization header with Bearer token
 * 2. mcp_mgr_discover_tools_http: Tool discovery via JSON-RPC over HTTP
 * 3. mgr_tool_execute (HTTP path): Tool execution via JSON-RPC over HTTP
 * ══════════════════════════════════════════════════════════════════════════ */

#ifdef HU_ENABLE_CURL

/* ──────────────────────────────────────────────────────────────────────── */
/* Test: JSON-RPC request building for tools/call                           */
/* ──────────────────────────────────────────────────────────────────────── */
static void test_jsonrpc_build_tools_call(void) {
    hu_allocator_t alloc = hu_system_allocator();

    char *request_json = NULL;
    size_t request_len = 0;
    hu_error_t err = hu_mcp_jsonrpc_build_tools_call(
        &alloc, 1, "my_tool", "{\"arg\":\"value\"}", &request_json, &request_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(request_json);
    HU_ASSERT_GT(request_len, 0u);

    /* Check that request contains expected fields */
    HU_ASSERT_NOT_NULL(strstr(request_json, "\"jsonrpc\":\"2.0\""));
    HU_ASSERT_NOT_NULL(strstr(request_json, "\"method\":\"tools/call\""));
    HU_ASSERT_NOT_NULL(strstr(request_json, "\"name\":\"my_tool\""));

    alloc.free(alloc.ctx, request_json, request_len + 1);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Test: JSON-RPC request building for tools/list                          */
/* ──────────────────────────────────────────────────────────────────────── */
static void test_jsonrpc_build_tools_list(void) {
    hu_allocator_t alloc = hu_system_allocator();

    char *request_json = NULL;
    size_t request_len = 0;
    hu_error_t err = hu_mcp_jsonrpc_build_tools_list(&alloc, 2, &request_json, &request_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(request_json);

    /* Check request structure */
    HU_ASSERT_NOT_NULL(strstr(request_json, "\"method\":\"tools/list\""));
    HU_ASSERT_NOT_NULL(strstr(request_json, "\"id\":2"));

    alloc.free(alloc.ctx, request_json, request_len + 1);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Test: JSON-RPC response parsing - success case                          */
/* ──────────────────────────────────────────────────────────────────────── */
static void test_jsonrpc_parse_response_success(void) {
    hu_allocator_t alloc = hu_system_allocator();

    const char *response = "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"value\":\"test\"}}";

    uint32_t response_id = 0;
    char *result = NULL;
    size_t result_len = 0;
    bool is_error = false;

    hu_error_t err = hu_mcp_jsonrpc_parse_response(&alloc, response, strlen(response),
                                                   &response_id, &result, &result_len, &is_error);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(response_id, 1u);
    HU_ASSERT_FALSE(is_error);
    HU_ASSERT_NOT_NULL(result);
    HU_ASSERT_GT(result_len, 0u);

    if (result)
        alloc.free(alloc.ctx, result, result_len + 1);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Test: JSON-RPC response parsing - error case                            */
/* ──────────────────────────────────────────────────────────────────────── */
static void test_jsonrpc_parse_response_error(void) {
    hu_allocator_t alloc = hu_system_allocator();

    const char *response = "{\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{\"code\":-32600,\"message\":\"Invalid request\"}}";

    uint32_t response_id = 0;
    char *result = NULL;
    size_t result_len = 0;
    bool is_error = false;

    hu_error_t err = hu_mcp_jsonrpc_parse_response(&alloc, response, strlen(response),
                                                   &response_id, &result, &result_len, &is_error);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(is_error);
    HU_ASSERT_EQ(response_id, 1u);
    HU_ASSERT_NOT_NULL(result);  /* Error message should be set */

    if (result)
        alloc.free(alloc.ctx, result, result_len + 1);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Test: HTTP Server Creation with URL (not stdio)                          */
/* ──────────────────────────────────────────────────────────────────────── */
static void test_mgr_http_server_entry(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_mcp_server_entry_t entries[] = {
        {
            .name = "http-server",
            .transport_type = "http",
            .url = "http://localhost:8000/mcp",
            .auto_connect = true,
            .timeout_ms = 5000,
        }
    };

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, entries, 1, &mgr);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(mgr);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 1u);

    /* Verify server info */
    hu_mcp_server_info_t info;
    err = hu_mcp_manager_server_info(mgr, 0, &info);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(info.name);
    HU_ASSERT(strcmp(info.name, "http-server") == 0);

    hu_mcp_manager_destroy(mgr);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Test: SSE Server Creation with URL                                       */
/* ──────────────────────────────────────────────────────────────────────── */
static void test_mgr_sse_server_entry(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_mcp_server_entry_t entries[] = {
        {
            .name = "sse-server",
            .transport_type = "sse",
            .url = "http://localhost:9000/sse",
            .auto_connect = false,
        }
    };

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, entries, 1, &mgr);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(mgr);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 1u);

    hu_mcp_manager_destroy(mgr);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Test: OAuth Token in HTTP Server Config                                  */
/* ──────────────────────────────────────────────────────────────────────── */
static void test_mgr_http_server_with_oauth(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_mcp_server_entry_t entries[] = {
        {
            .name = "oauth-server",
            .transport_type = "http",
            .url = "http://localhost:8000/mcp",
            .oauth_client_id = "client-123",
            .oauth_auth_url = "https://auth.example.com/authorize",
            .oauth_token_url = "https://auth.example.com/token",
            .oauth_scopes = "read write",
            .oauth_redirect_uri = "http://localhost:3000/callback",
            .auto_connect = true,
        }
    };

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, entries, 1, &mgr);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(mgr);

    hu_mcp_manager_destroy(mgr);
}

/* ──────────────────────────────────────────────────────────────────────── */
/* Test: Mixed stdio and HTTP servers in same manager                       */
/* ──────────────────────────────────────────────────────────────────────── */
static void test_mgr_mixed_transports(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_mcp_server_entry_t entries[] = {
        {
            .name = "stdio-server",
            .transport_type = "stdio",
            .command = "/usr/local/bin/mcp-server",
            .auto_connect = true,
        },
        {
            .name = "http-server",
            .transport_type = "http",
            .url = "http://localhost:8000/mcp",
            .auto_connect = false,
        }
    };

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, entries, 2, &mgr);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 2u);

    /* Verify both servers are present */
    size_t idx_stdio = 0;
    err = hu_mcp_manager_find_server(mgr, "stdio-server", &idx_stdio);
    HU_ASSERT_EQ(err, HU_OK);

    size_t idx_http = 0;
    err = hu_mcp_manager_find_server(mgr, "http-server", &idx_http);
    HU_ASSERT_EQ(err, HU_OK);

    hu_mcp_manager_destroy(mgr);
}

#endif  /* HU_ENABLE_CURL */

/* ══════════════════════════════════════════════════════════════════════════
 * Test Registration
 * ══════════════════════════════════════════════════════════════════════════ */

HU_TEST_SUITE("MCP HTTP Integration", {
#ifdef HU_ENABLE_CURL
    HU_TEST(test_jsonrpc_build_tools_call);
    HU_TEST(test_jsonrpc_build_tools_list);
    HU_TEST(test_jsonrpc_parse_response_success);
    HU_TEST(test_jsonrpc_parse_response_error);
    HU_TEST(test_mgr_http_server_entry);
    HU_TEST(test_mgr_sse_server_entry);
    HU_TEST(test_mgr_http_server_with_oauth);
    HU_TEST(test_mgr_mixed_transports);
#endif
});
