#include "test_framework.h"
#include "seaclaw/mcp.h"
#include "seaclaw/mcp_server.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/tool.h"
#include "seaclaw/memory.h"
#include <string.h>

static void test_mcp_server_create_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_config_t cfg = { .command = "echo", .args = NULL, .args_count = 0 };
    sc_mcp_server_t *srv = sc_mcp_server_create(&alloc, &cfg);
    SC_ASSERT_NOT_NULL(srv);
    sc_mcp_server_destroy(srv);
}

static void test_mcp_server_connect_test_mode(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_config_t cfg = { .command = "echo", .args = NULL, .args_count = 0 };
    sc_mcp_server_t *srv = sc_mcp_server_create(&alloc, &cfg);
    SC_ASSERT_NOT_NULL(srv);
    sc_error_t err = sc_mcp_server_connect(srv);
    SC_ASSERT_EQ(err, SC_OK);
    sc_mcp_server_destroy(srv);
}

static void test_mcp_server_list_tools_mock(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_config_t cfg = { .command = "echo", .args = NULL, .args_count = 0 };
    sc_mcp_server_t *srv = sc_mcp_server_create(&alloc, &cfg);
    sc_mcp_server_connect(srv);
    char **names = NULL, **descs = NULL, **params = NULL;
    size_t count = 0;
    sc_error_t err = sc_mcp_server_list_tools(srv, &alloc, &names, &descs, &params, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(count, 1u);
    SC_ASSERT_STR_EQ(names[0], "mock_tool");
    SC_ASSERT_NOT_NULL(params[0]);
    SC_ASSERT_TRUE(strlen(params[0]) > 2);
    for (size_t i = 0; i < count; i++) {
        alloc.free(alloc.ctx, names[i], strlen(names[i]) + 1);
        alloc.free(alloc.ctx, descs[i], strlen(descs[i]) + 1);
        if (params[i])
            alloc.free(alloc.ctx, params[i], strlen(params[i]) + 1);
    }
    alloc.free(alloc.ctx, names, count * sizeof(char *));
    alloc.free(alloc.ctx, descs, count * sizeof(char *));
    alloc.free(alloc.ctx, params, count * sizeof(char *));
    sc_mcp_server_destroy(srv);
}

static void test_mcp_server_call_tool_mock(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_config_t cfg = { .command = "echo", .args = NULL, .args_count = 0 };
    sc_mcp_server_t *srv = sc_mcp_server_create(&alloc, &cfg);
    sc_mcp_server_connect(srv);
    char *result = NULL;
    size_t result_len = 0;
    sc_error_t err = sc_mcp_server_call_tool(srv, &alloc, "mock_tool", "{}",
        &result, &result_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(result);
    alloc.free(alloc.ctx, result, result_len + 1);
    sc_mcp_server_destroy(srv);
}

static void test_mcp_init_tools_mock(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_config_t cfg = { .command = "echo", .args = NULL, .args_count = 0 };
    sc_tool_t *tools = NULL;
    size_t count = 0;
    sc_error_t err = sc_mcp_init_tools(&alloc, &cfg, 1, &tools, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(count >= 1);
    sc_mcp_free_tools(&alloc, tools, count);
}

/* ── MCP Host (server mode) tests ────────────────────────────────────────── */

static sc_error_t mock_tool_execute(void *ctx, sc_allocator_t *alloc,
    const sc_json_value_t *args, sc_tool_result_t *out)
{
    (void)ctx; (void)args;
    const char *msg = "mock ok";
    size_t len = strlen(msg);
    char *buf = (char *)alloc->alloc(alloc->ctx, len + 1);
    if (!buf) return SC_ERR_OUT_OF_MEMORY;
    memcpy(buf, msg, len + 1);
    *out = sc_tool_result_ok_owned(buf, len);
    return SC_OK;
}
static const char *mock_tool_name(void *ctx) { (void)ctx; return "test_tool"; }
static const char *mock_tool_desc(void *ctx) { (void)ctx; return "A test tool"; }
static const char *mock_tool_params(void *ctx) { (void)ctx; return "{\"type\":\"object\",\"properties\":{}}"; }

static const sc_tool_vtable_t mock_host_vtable = {
    .execute = mock_tool_execute,
    .name = mock_tool_name,
    .description = mock_tool_desc,
    .parameters_json = mock_tool_params,
    .deinit = NULL,
};

static void test_mcp_host_create_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tools[1];
    tools[0].ctx = NULL;
    tools[0].vtable = &mock_host_vtable;

    sc_mcp_host_t *host = NULL;
    sc_error_t err = sc_mcp_host_create(&alloc, tools, 1, NULL, &host);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(host);
    sc_mcp_host_destroy(host);
}

static void test_mcp_host_create_null_alloc(void) {
    sc_mcp_host_t *host = NULL;
    sc_error_t err = sc_mcp_host_create(NULL, NULL, 0, NULL, &host);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_mcp_host_create_null_out(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_mcp_host_create(&alloc, NULL, 0, NULL, NULL);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_mcp_host_create_zero_tools(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_host_t *host = NULL;
    sc_error_t err = sc_mcp_host_create(&alloc, NULL, 0, NULL, &host);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(host);
    sc_mcp_host_destroy(host);
}

static void test_mcp_host_run_null(void) {
    sc_error_t err = sc_mcp_host_run(NULL);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_mcp_host_destroy_null(void) {
    sc_mcp_host_destroy(NULL); /* should not crash */
}

static void test_mcp_host_create_with_memory(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_t mem = sc_none_memory_create(&alloc);
    SC_ASSERT_NOT_NULL(mem.vtable);
    sc_tool_t tools[1];
    tools[0].ctx = NULL;
    tools[0].vtable = &mock_host_vtable;
    sc_mcp_host_t *host = NULL;
    sc_error_t err = sc_mcp_host_create(&alloc, tools, 1, &mem, &host);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(host);
    sc_mcp_host_destroy(host);
    if (mem.vtable && mem.vtable->deinit) mem.vtable->deinit(mem.ctx);
}

static void test_mcp_server_null_config_fails(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_t *srv = sc_mcp_server_create(&alloc, NULL);
    SC_ASSERT_NULL(srv);
}

static void test_mcp_server_call_tool_nonexistent(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_config_t cfg = { .command = "echo", .args = NULL, .args_count = 0 };
    sc_mcp_server_t *srv = sc_mcp_server_create(&alloc, &cfg);
    SC_ASSERT_NOT_NULL(srv);
    sc_mcp_server_connect(srv);
    char *result = NULL;
    size_t result_len = 0;
    sc_error_t err = sc_mcp_server_call_tool(srv, &alloc, "nonexistent_tool", "{}",
        &result, &result_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(result);
    alloc.free(alloc.ctx, result, result_len + 1);
    sc_mcp_server_destroy(srv);
}

static void test_mcp_server_double_connect(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_config_t cfg = { .command = "echo", .args = NULL, .args_count = 0 };
    sc_mcp_server_t *srv = sc_mcp_server_create(&alloc, &cfg);
    SC_ASSERT_NOT_NULL(srv);
    sc_error_t err1 = sc_mcp_server_connect(srv);
    sc_error_t err2 = sc_mcp_server_connect(srv);
    SC_ASSERT_EQ(err1, SC_OK);
    SC_ASSERT_EQ(err2, SC_OK);
    sc_mcp_server_destroy(srv);
}

static void test_mcp_init_tools_null_alloc(void) {
    sc_tool_t *tools = NULL;
    size_t count = 0;
    sc_error_t err = sc_mcp_init_tools(NULL, NULL, 0, &tools, &count);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_mcp_init_tools_zero_configs(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t *tools = NULL;
    size_t count = 0;
    sc_error_t err = sc_mcp_init_tools(&alloc, NULL, 0, &tools, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NULL(tools);
    SC_ASSERT_EQ(count, 0u);
}

static void test_mcp_host_create_many_tools(void) {
    sc_allocator_t alloc = sc_system_allocator();
    static const sc_tool_vtable_t vt = {
        .execute = mock_tool_execute,
        .name = mock_tool_name,
        .description = mock_tool_desc,
        .parameters_json = mock_tool_params,
        .deinit = NULL,
    };
    sc_tool_t tools[6];
    for (size_t i = 0; i < 6; i++) {
        tools[i].ctx = NULL;
        tools[i].vtable = &vt;
    }
    sc_mcp_host_t *host = NULL;
    sc_error_t err = sc_mcp_host_create(&alloc, tools, 6, NULL, &host);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(host);
    sc_mcp_host_destroy(host);
}

static void test_mcp_server_list_tools_not_connected(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_config_t cfg = { .command = "echo", .args = NULL, .args_count = 0 };
    sc_mcp_server_t *srv = sc_mcp_server_create(&alloc, &cfg);
    SC_ASSERT_NOT_NULL(srv);
    char **names = NULL, **descs = NULL, **params = NULL;
    size_t count = 0;
    sc_error_t err = sc_mcp_server_list_tools(srv, &alloc, &names, &descs, &params, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(count >= 1);
    for (size_t i = 0; i < count; i++) {
        alloc.free(alloc.ctx, names[i], strlen(names[i]) + 1);
        alloc.free(alloc.ctx, descs[i], strlen(descs[i]) + 1);
        if (params && params[i])
            alloc.free(alloc.ctx, params[i], strlen(params[i]) + 1);
    }
    alloc.free(alloc.ctx, names, count * sizeof(char *));
    alloc.free(alloc.ctx, descs, count * sizeof(char *));
    if (params)
        alloc.free(alloc.ctx, params, count * sizeof(char *));
    sc_mcp_server_destroy(srv);
}

static void test_mcp_init_tools_null_out_rejected(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_config_t cfg = { .command = "echo", .args = NULL, .args_count = 0 };
    sc_tool_t *tools = NULL;
    size_t count = 0;
    sc_error_t err = sc_mcp_init_tools(&alloc, &cfg, 1, NULL, &count);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
    err = sc_mcp_init_tools(&alloc, &cfg, 1, &tools, NULL);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_mcp_host_create_null_alloc_rejected(void) {
    sc_tool_t tools[1] = {{ .ctx = NULL, .vtable = &mock_host_vtable }};
    sc_mcp_host_t *host = NULL;
    sc_error_t err = sc_mcp_host_create(NULL, tools, 1, NULL, &host);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_mcp_server_create_null_command_fails(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_config_t cfg = { .command = NULL, .args = NULL, .args_count = 0 };
    sc_mcp_server_t *srv = sc_mcp_server_create(&alloc, &cfg);
    SC_ASSERT_NULL(srv);
}

static void test_mcp_server_create_null_alloc_fails(void) {
    sc_mcp_server_config_t cfg = { .command = "echo", .args = NULL, .args_count = 0 };
    sc_mcp_server_t *srv = sc_mcp_server_create(NULL, &cfg);
    SC_ASSERT_NULL(srv);
}

static void test_mcp_free_tools_null_safe(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_free_tools(&alloc, NULL, 0);
}

void run_mcp_tests(void) {
    SC_TEST_SUITE("MCP Client");
    SC_RUN_TEST(test_mcp_server_create_destroy);
    SC_RUN_TEST(test_mcp_server_connect_test_mode);
    SC_RUN_TEST(test_mcp_server_list_tools_mock);
    SC_RUN_TEST(test_mcp_server_call_tool_mock);
    SC_RUN_TEST(test_mcp_init_tools_mock);

    SC_TEST_SUITE("MCP Host (Server)");
    SC_RUN_TEST(test_mcp_host_create_destroy);
    SC_RUN_TEST(test_mcp_host_create_null_alloc);
    SC_RUN_TEST(test_mcp_host_create_null_out);
    SC_RUN_TEST(test_mcp_host_create_zero_tools);
    SC_RUN_TEST(test_mcp_host_run_null);
    SC_RUN_TEST(test_mcp_host_destroy_null);
    SC_RUN_TEST(test_mcp_host_create_with_memory);
    SC_RUN_TEST(test_mcp_host_create_many_tools);
    SC_RUN_TEST(test_mcp_host_create_null_alloc_rejected);
    SC_RUN_TEST(test_mcp_server_null_config_fails);
    SC_RUN_TEST(test_mcp_server_create_null_command_fails);
    SC_RUN_TEST(test_mcp_server_create_null_alloc_fails);
    SC_RUN_TEST(test_mcp_server_call_tool_nonexistent);
    SC_RUN_TEST(test_mcp_server_double_connect);
    SC_RUN_TEST(test_mcp_server_list_tools_not_connected);
    SC_RUN_TEST(test_mcp_init_tools_null_alloc);
    SC_RUN_TEST(test_mcp_init_tools_zero_configs);
    SC_RUN_TEST(test_mcp_init_tools_null_out_rejected);
    SC_RUN_TEST(test_mcp_free_tools_null_safe);
}
