#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/mcp.h"
#include "human/mcp_manager.h"
#include "human/tool.h"
#include "test_framework.h"
#include <string.h>

/* ── Helper: build a server entry ─────────────────────────────────────── */

static hu_mcp_server_entry_t make_entry(const char *name, const char *command, bool auto_connect,
                                        uint32_t timeout_ms) {
    hu_mcp_server_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name = (char *)name;
    e.command = (char *)command;
    e.auto_connect = auto_connect;
    e.timeout_ms = timeout_ms;
    return e;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Tests
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1. Create with NULL allocator fails */
static void test_mgr_create_null_alloc(void) {
    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(NULL, NULL, 0, &mgr);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_NULL(mgr);
}

/* 2. Create with NULL out fails */
static void test_mgr_create_null_out(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_mcp_manager_create(&alloc, NULL, 0, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

/* 3. Create with zero entries succeeds */
static void test_mgr_create_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, NULL, 0, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(mgr);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 0u);
    hu_mcp_manager_destroy(mgr);
}

/* 4. Destroy NULL is safe */
static void test_mgr_destroy_null(void) {
    hu_mcp_manager_destroy(NULL); /* must not crash */
}

/* 5. Create with one entry, verify server count */
static void test_mgr_create_one_server(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e = make_entry("test-server", "echo", true, 5000);
    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(mgr);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 1u);
    hu_mcp_manager_destroy(mgr);
}

/* 6. Create with multiple entries */
static void test_mgr_create_multiple_servers(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t entries[3] = {
        make_entry("alpha", "echo", true, 0),
        make_entry("beta", "echo", false, 10000),
        make_entry("gamma", "echo", true, 0),
    };
    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, entries, 3, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 3u);
    hu_mcp_manager_destroy(mgr);
}

/* 7. Entries with NULL name or command are skipped */
static void test_mgr_create_skips_invalid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t entries[3] = {
        make_entry("good", "echo", true, 0),
        make_entry(NULL, "echo", true, 0),   /* no name */
        make_entry("also-good", "echo", true, 0),
    };
    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, entries, 3, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 2u);
    hu_mcp_manager_destroy(mgr);
}

/* 8. Find server by name */
static void test_mgr_find_server(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t entries[2] = {
        make_entry("alpha", "echo", true, 0),
        make_entry("beta", "echo", false, 0),
    };
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, entries, 2, &mgr);

    size_t idx = 999;
    hu_error_t err = hu_mcp_manager_find_server(mgr, "beta", &idx);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(idx, 1u);

    err = hu_mcp_manager_find_server(mgr, "nonexistent", &idx);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);

    hu_mcp_manager_destroy(mgr);
}

/* 9. Server info query */
static void test_mgr_server_info(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e = make_entry("info-test", "echo", false, 7500);
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, &e, 1, &mgr);

    hu_mcp_server_info_t info;
    hu_error_t err = hu_mcp_manager_server_info(mgr, 0, &info);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(info.name, "info-test");
    HU_ASSERT_FALSE(info.connected);
    HU_ASSERT_EQ(info.timeout_ms, 7500u);

    /* Out of range */
    err = hu_mcp_manager_server_info(mgr, 99, &info);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);

    hu_mcp_manager_destroy(mgr);
}

/* 10. Default timeout applied when timeout_ms is 0 */
static void test_mgr_default_timeout(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e = make_entry("default-to", "echo", true, 0);
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, &e, 1, &mgr);

    hu_mcp_server_info_t info;
    hu_mcp_manager_server_info(mgr, 0, &info);
    HU_ASSERT_EQ(info.timeout_ms, HU_MCP_MANAGER_DEFAULT_TIMEOUT);

    hu_mcp_manager_destroy(mgr);
}

/* 11. Connect auto — connects auto_connect servers (test mode succeeds) */
static void test_mgr_connect_auto(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t entries[2] = {
        make_entry("auto-yes", "echo", true, 0),
        make_entry("auto-no", "echo", false, 0),
    };
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, entries, 2, &mgr);

    hu_error_t err = hu_mcp_manager_connect_auto(mgr);
    HU_ASSERT_EQ(err, HU_OK);

    hu_mcp_server_info_t info;
    hu_mcp_manager_server_info(mgr, 0, &info);
    HU_ASSERT_TRUE(info.connected);

    hu_mcp_manager_server_info(mgr, 1, &info);
    HU_ASSERT_FALSE(info.connected);

    hu_mcp_manager_destroy(mgr);
}

/* 12. Connect specific server by name */
static void test_mgr_connect_server_by_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e = make_entry("named", "echo", false, 0);
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, &e, 1, &mgr);

    hu_error_t err = hu_mcp_manager_connect_server(mgr, "named");
    HU_ASSERT_EQ(err, HU_OK);

    hu_mcp_server_info_t info;
    hu_mcp_manager_server_info(mgr, 0, &info);
    HU_ASSERT_TRUE(info.connected);

    /* Connecting again is OK */
    err = hu_mcp_manager_connect_server(mgr, "named");
    HU_ASSERT_EQ(err, HU_OK);

    hu_mcp_manager_destroy(mgr);
}

/* 13. Connect nonexistent server returns NOT_FOUND */
static void test_mgr_connect_nonexistent(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, NULL, 0, &mgr);

    hu_error_t err = hu_mcp_manager_connect_server(mgr, "does-not-exist");
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);

    hu_mcp_manager_destroy(mgr);
}

/* 14. Load tools from connected server — mock returns mcp__<name>__mock_tool */
static void test_mgr_load_tools(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e = make_entry("myserver", "echo", true, 0);
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    hu_mcp_manager_connect_auto(mgr);

    hu_tool_t *tools = NULL;
    size_t count = 0;
    hu_error_t err = hu_mcp_manager_load_tools(mgr, &alloc, &tools, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(count >= 1);

    /* Verify naming convention */
    const char *name = tools[0].vtable->name(tools[0].ctx);
    HU_ASSERT_NOT_NULL(name);
    HU_ASSERT_STR_EQ(name, "mcp__myserver__mock_tool");

    /* Verify description is populated */
    const char *desc = tools[0].vtable->description(tools[0].ctx);
    HU_ASSERT_NOT_NULL(desc);
    HU_ASSERT_TRUE(strlen(desc) > 0);

    /* Verify params JSON is valid */
    const char *params = tools[0].vtable->parameters_json(tools[0].ctx);
    HU_ASSERT_NOT_NULL(params);
    HU_ASSERT_TRUE(strlen(params) > 2);

    hu_mcp_manager_free_tools(&alloc, tools, count);
    hu_mcp_manager_destroy(mgr);
}

/* 15. Load tools with no connected servers returns empty array */
static void test_mgr_load_tools_none_connected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e = make_entry("disconnected", "echo", false, 0);
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, &e, 1, &mgr);

    hu_tool_t *tools = NULL;
    size_t count = 0;
    hu_error_t err = hu_mcp_manager_load_tools(mgr, &alloc, &tools, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 0u);

    hu_mcp_manager_destroy(mgr);
}

/* 16. Execute MCP tool through vtable */
static void test_mgr_tool_execute(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e = make_entry("exec-test", "echo", true, 0);
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    hu_mcp_manager_connect_auto(mgr);

    hu_tool_t *tools = NULL;
    size_t count = 0;
    hu_mcp_manager_load_tools(mgr, &alloc, &tools, &count);
    HU_ASSERT_TRUE(count >= 1);

    hu_tool_result_t result;
    hu_error_t err = tools[0].vtable->execute(tools[0].ctx, &alloc, NULL, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT_TRUE(result.output_len > 0);

    hu_tool_result_free(&alloc, &result);
    hu_mcp_manager_free_tools(&alloc, tools, count);
    hu_mcp_manager_destroy(mgr);
}

/* 17. Direct call_tool by server name */
static void test_mgr_call_tool_direct(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e = make_entry("direct", "echo", true, 0);
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    hu_mcp_manager_connect_auto(mgr);

    char *result = NULL;
    size_t result_len = 0;
    hu_error_t err =
        hu_mcp_manager_call_tool(mgr, &alloc, "direct", "mock_tool", "{}", &result, &result_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(result);
    HU_ASSERT_TRUE(result_len > 0);

    alloc.free(alloc.ctx, result, result_len + 1);
    hu_mcp_manager_destroy(mgr);
}

/* 18. Direct call_tool to nonexistent server returns NOT_FOUND */
static void test_mgr_call_tool_no_server(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, NULL, 0, &mgr);

    char *result = NULL;
    size_t result_len = 0;
    hu_error_t err =
        hu_mcp_manager_call_tool(mgr, &alloc, "nope", "tool", "{}", &result, &result_len);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);

    hu_mcp_manager_destroy(mgr);
}

/* 19. Direct call_tool to disconnected server returns IO error */
static void test_mgr_call_tool_disconnected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e = make_entry("offline", "echo", false, 0);
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, &e, 1, &mgr);

    char *result = NULL;
    size_t result_len = 0;
    hu_error_t err =
        hu_mcp_manager_call_tool(mgr, &alloc, "offline", "tool", "{}", &result, &result_len);
    HU_ASSERT_EQ(err, HU_ERR_IO);

    hu_mcp_manager_destroy(mgr);
}

/* 20. Load tools NULL args rejected */
static void test_mgr_load_tools_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_mcp_manager_load_tools(NULL, &alloc, NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

/* 21. Free tools NULL-safe */
static void test_mgr_free_tools_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_manager_free_tools(&alloc, NULL, 0); /* must not crash */
}

/* 22. Server count on NULL manager */
static void test_mgr_server_count_null(void) {
    HU_ASSERT_EQ(hu_mcp_manager_server_count(NULL), 0u);
}

/* 23. Tracking allocator: verify zero leaks */
static void test_mgr_no_leaks(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    HU_ASSERT_NOT_NULL(ta);
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_mcp_server_entry_t entries[2] = {
        make_entry("leak-a", "echo", true, 0),
        make_entry("leak-b", "echo", true, 5000),
    };
    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, entries, 2, &mgr);
    HU_ASSERT_EQ(err, HU_OK);

    hu_mcp_manager_connect_auto(mgr);

    hu_tool_t *tools = NULL;
    size_t count = 0;
    hu_mcp_manager_load_tools(mgr, &alloc, &tools, &count);

    /* Execute each tool */
    for (size_t i = 0; i < count; i++) {
        hu_tool_result_t result;
        tools[i].vtable->execute(tools[i].ctx, &alloc, NULL, &result);
        hu_tool_result_free(&alloc, &result);
    }

    hu_mcp_manager_free_tools(&alloc, tools, count);
    hu_mcp_manager_destroy(mgr);

    size_t leaks = hu_tracking_allocator_leaks(ta);
    hu_tracking_allocator_destroy(ta);
    HU_ASSERT_EQ(leaks, 0u);
}

/* 24. Multiple servers produce unique tool names */
static void test_mgr_unique_tool_names(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t entries[2] = {
        make_entry("srv-a", "echo", true, 0),
        make_entry("srv-b", "echo", true, 0),
    };
    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, entries, 2, &mgr);
    hu_mcp_manager_connect_auto(mgr);

    hu_tool_t *tools = NULL;
    size_t count = 0;
    hu_mcp_manager_load_tools(mgr, &alloc, &tools, &count);
    HU_ASSERT_EQ(count, 2u);

    const char *n0 = tools[0].vtable->name(tools[0].ctx);
    const char *n1 = tools[1].vtable->name(tools[1].ctx);
    HU_ASSERT_NOT_NULL(n0);
    HU_ASSERT_NOT_NULL(n1);
    /* Names must differ because server names differ */
    HU_ASSERT_TRUE(strcmp(n0, n1) != 0);
    HU_ASSERT_STR_CONTAINS(n0, "mcp__srv-a__");
    HU_ASSERT_STR_CONTAINS(n1, "mcp__srv-b__");

    hu_mcp_manager_free_tools(&alloc, tools, count);
    hu_mcp_manager_destroy(mgr);
}

/* 25. Connect auto with NULL manager fails gracefully */
static void test_mgr_connect_auto_null(void) {
    hu_error_t err = hu_mcp_manager_connect_auto(NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

/* ── Transport Type Tests ─────────────────────────────────────────── */

/* 26. HTTP transport: create with valid URL succeeds */
static void test_mgr_http_transport_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name = "http-server";
    e.transport_type = "http";
    e.url = "http://localhost:8000/mcp";
    e.auto_connect = true;
    e.timeout_ms = 0;

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(mgr);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 1u);

    hu_mcp_server_info_t info;
    hu_mcp_manager_server_info(mgr, 0, &info);
    HU_ASSERT_STR_EQ(info.name, "http-server");

    hu_mcp_manager_destroy(mgr);
}

/* 27. SSE transport: create with valid URL succeeds */
static void test_mgr_sse_transport_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name = "sse-server";
    e.transport_type = "sse";
    e.url = "http://localhost:9000/events";
    e.auto_connect = false;
    e.timeout_ms = 15000;

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 1u);

    hu_mcp_server_info_t info;
    hu_mcp_manager_server_info(mgr, 0, &info);
    HU_ASSERT_STR_EQ(info.name, "sse-server");
    HU_ASSERT_EQ(info.timeout_ms, 15000u);

    hu_mcp_manager_destroy(mgr);
}

/* 28. HTTP transport without URL is skipped */
static void test_mgr_http_transport_no_url(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name = "http-no-url";
    e.transport_type = "http";
    e.url = NULL;  /* Missing URL */
    e.auto_connect = true;

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 0u);  /* Entry skipped */

    hu_mcp_manager_destroy(mgr);
}

/* 29. Stdio transport validation: command required */
static void test_mgr_stdio_transport_no_command(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name = "stdio-no-cmd";
    e.transport_type = "stdio";
    e.command = NULL;  /* Missing command */

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 0u);  /* Entry skipped */

    hu_mcp_manager_destroy(mgr);
}

/* 30. Invalid transport type rejected */
static void test_mgr_invalid_transport_type(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name = "invalid-transport";
    e.transport_type = "websocket";  /* Invalid type */
    e.url = "ws://localhost";

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 0u);  /* Entry skipped */

    hu_mcp_manager_destroy(mgr);
}

/* 31. Mixed transport types in one manager */
static void test_mgr_mixed_transport_types(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t entries[3] = {
        {.name = "stdio-srv", .transport_type = "stdio", .command = "echo", .auto_connect = true},
        {.name = "http-srv", .transport_type = "http", .url = "http://localhost:8000", .auto_connect = false},
        {.name = "sse-srv", .transport_type = "sse", .url = "http://localhost:9000", .auto_connect = true},
    };

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, entries, 3, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 3u);

    hu_mcp_server_info_t info;
    hu_mcp_manager_server_info(mgr, 0, &info);
    HU_ASSERT_STR_EQ(info.name, "stdio-srv");

    hu_mcp_manager_server_info(mgr, 1, &info);
    HU_ASSERT_STR_EQ(info.name, "http-srv");

    hu_mcp_manager_server_info(mgr, 2, &info);
    HU_ASSERT_STR_EQ(info.name, "sse-srv");

    hu_mcp_manager_destroy(mgr);
}

/* 32. HTTP transport connect (test mode) */
static void test_mgr_http_transport_connect(void) {
#ifdef HU_HTTP_CURL
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name = "http-connect";
    e.transport_type = "http";
    e.url = "http://localhost:8765/mcp";  /* Nonexistent endpoint, but should create transport */
    e.auto_connect = true;

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    HU_ASSERT_EQ(err, HU_OK);

    hu_mcp_manager_connect_auto(mgr);

    hu_mcp_server_info_t info;
    hu_mcp_manager_server_info(mgr, 0, &info);
    HU_ASSERT_TRUE(info.connected);

    hu_mcp_manager_destroy(mgr);
#endif
}

/* 33. SSE transport connect (test mode) */
static void test_mgr_sse_transport_connect(void) {
#ifdef HU_HTTP_CURL
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name = "sse-connect";
    e.transport_type = "sse";
    e.url = "http://localhost:8766/events";
    e.auto_connect = true;

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    HU_ASSERT_EQ(err, HU_OK);

    hu_mcp_manager_connect_auto(mgr);

    hu_mcp_server_info_t info;
    hu_mcp_manager_server_info(mgr, 0, &info);
    HU_ASSERT_TRUE(info.connected);

    hu_mcp_manager_destroy(mgr);
#endif
}

/* 34. Default transport type (NULL) treated as stdio */
static void test_mgr_default_transport_type(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name = "default-transport";
    e.transport_type = NULL;  /* Should default to stdio */
    e.command = "echo";
    e.auto_connect = true;

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), 1u);

    hu_mcp_manager_connect_auto(mgr);

    hu_mcp_server_info_t info;
    hu_mcp_manager_server_info(mgr, 0, &info);
    HU_ASSERT_TRUE(info.connected);

    hu_mcp_manager_destroy(mgr);
}

/* 35. Transport-based tool execution returns NOT_SUPPORTED */
static void test_mgr_http_tool_execution_unsupported(void) {
#ifdef HU_HTTP_CURL
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_server_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name = "http-exec";
    e.transport_type = "http";
    e.url = "http://localhost:8767/mcp";
    e.auto_connect = true;

    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    hu_mcp_manager_connect_auto(mgr);

    char *result = NULL;
    size_t result_len = 0;
    hu_error_t err = hu_mcp_manager_call_tool(mgr, &alloc, "http-exec", "some_tool", "{}", &result, &result_len);
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);

    hu_mcp_manager_destroy(mgr);
#endif
}

/* 36. Tracking allocator with HTTP transport: verify zero leaks */
static void test_mgr_http_transport_no_leaks(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    HU_ASSERT_NOT_NULL(ta);
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_mcp_server_entry_t e;
    memset(&e, 0, sizeof(e));
    e.name = "http-leak-test";
    e.transport_type = "http";
    e.url = "http://localhost:8768/mcp";
    e.auto_connect = true;

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, &e, 1, &mgr);
    HU_ASSERT_EQ(err, HU_OK);

    hu_mcp_manager_connect_auto(mgr);
    hu_mcp_manager_destroy(mgr);

    size_t leaks = hu_tracking_allocator_leaks(ta);
    hu_tracking_allocator_destroy(ta);
    HU_ASSERT_EQ(leaks, 0u);
}

/* ── Test runner ──────────────────────────────────────────────────────── */

void run_mcp_manager_tests(void) {
    HU_TEST_SUITE("MCP Manager");
    HU_RUN_TEST(test_mgr_create_null_alloc);
    HU_RUN_TEST(test_mgr_create_null_out);
    HU_RUN_TEST(test_mgr_create_empty);
    HU_RUN_TEST(test_mgr_destroy_null);
    HU_RUN_TEST(test_mgr_create_one_server);
    HU_RUN_TEST(test_mgr_create_multiple_servers);
    HU_RUN_TEST(test_mgr_create_skips_invalid);
    HU_RUN_TEST(test_mgr_find_server);
    HU_RUN_TEST(test_mgr_server_info);
    HU_RUN_TEST(test_mgr_default_timeout);
    HU_RUN_TEST(test_mgr_connect_auto);
    HU_RUN_TEST(test_mgr_connect_server_by_name);
    HU_RUN_TEST(test_mgr_connect_nonexistent);
    HU_RUN_TEST(test_mgr_load_tools);
    HU_RUN_TEST(test_mgr_load_tools_none_connected);
    HU_RUN_TEST(test_mgr_tool_execute);
    HU_RUN_TEST(test_mgr_call_tool_direct);
    HU_RUN_TEST(test_mgr_call_tool_no_server);
    HU_RUN_TEST(test_mgr_call_tool_disconnected);
    HU_RUN_TEST(test_mgr_load_tools_null_args);
    HU_RUN_TEST(test_mgr_free_tools_null);
    HU_RUN_TEST(test_mgr_server_count_null);
    HU_RUN_TEST(test_mgr_no_leaks);
    HU_RUN_TEST(test_mgr_unique_tool_names);
    HU_RUN_TEST(test_mgr_connect_auto_null);
    HU_RUN_TEST(test_mgr_http_transport_create);
    HU_RUN_TEST(test_mgr_sse_transport_create);
    HU_RUN_TEST(test_mgr_http_transport_no_url);
    HU_RUN_TEST(test_mgr_stdio_transport_no_command);
    HU_RUN_TEST(test_mgr_invalid_transport_type);
    HU_RUN_TEST(test_mgr_mixed_transport_types);
    HU_RUN_TEST(test_mgr_http_transport_connect);
    HU_RUN_TEST(test_mgr_sse_transport_connect);
    HU_RUN_TEST(test_mgr_default_transport_type);
    HU_RUN_TEST(test_mgr_http_tool_execution_unsupported);
    HU_RUN_TEST(test_mgr_http_transport_no_leaks);
}
