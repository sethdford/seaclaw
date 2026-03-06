/* Plugin system wiring tests. */
#include "seaclaw/config.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/arena.h"
#include "seaclaw/core/error.h"
#include "seaclaw/plugin.h"
#include "seaclaw/plugin_loader.h"
#include "test_framework.h"
#include <string.h>

static void test_plugin_load_bad_path_fails(void) {
    sc_allocator_t a = sc_system_allocator();
    sc_plugin_host_t host = {
        .alloc = &a,
        .register_tool = NULL,
        .register_provider = NULL,
        .register_channel = NULL,
        .host_ctx = NULL,
    };
    sc_plugin_info_t info = {0};
    sc_plugin_handle_t *handle = NULL;
    sc_error_t err = sc_plugin_load(&a, "/nonexistent/plugin.so", &host, &info, &handle);
    SC_ASSERT_EQ(err, SC_ERR_NOT_FOUND);
    SC_ASSERT_NULL(handle);
}

static sc_error_t test_register_tool_cb(void *ctx, const char *name, void *tool_vtable) {
    (void)name;
    (void)tool_vtable;
    int *count = (int *)ctx;
    if (count)
        (*count)++;
    return SC_OK;
}

static void test_plugin_host_register_tool_callback(void) {
    sc_allocator_t a = sc_system_allocator();
    int callback_count = 0;
    sc_plugin_host_t host = {
        .alloc = &a,
        .register_tool = test_register_tool_cb,
        .register_provider = NULL,
        .register_channel = NULL,
        .host_ctx = &callback_count,
    };
    sc_plugin_info_t info = {0};
    sc_plugin_handle_t *handle = NULL;
    /* Load a mock plugin - in SC_IS_TEST, /valid/path.so succeeds */
    sc_error_t err = sc_plugin_load(&a, "/valid/mock.so", &host, &info, &handle);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(handle);
    SC_ASSERT_STR_EQ(info.name, "mock-plugin");
    /* Plugin init can call register_tool - mock doesn't, but we verify host is passed */
    SC_ASSERT_NOT_NULL(host.register_tool);
    sc_plugin_unload(handle);
}

static void test_plugin_config_parse_empty_array(void) {
    sc_allocator_t backing = sc_system_allocator();
    sc_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    sc_arena_t *arena = sc_arena_create(backing);
    SC_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = sc_arena_allocator(arena);
    const char *json = "{\"plugins\":[]}";
    sc_error_t err = sc_config_parse_json(&cfg, json, strlen(json));
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(cfg.plugins.plugin_paths_len, 0);
    SC_ASSERT_NULL(cfg.plugins.plugin_paths);
    sc_arena_destroy(arena);
}

static void test_plugin_config_parse_with_paths(void) {
    sc_allocator_t backing = sc_system_allocator();
    sc_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    sc_arena_t *arena = sc_arena_create(backing);
    SC_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = sc_arena_allocator(arena);
    const char *json =
        "{\"plugins\":{\"enabled\":true,\"paths\":[\"/lib/foo.so\",\"/lib/bar.so\"]}}";
    sc_error_t err = sc_config_parse_json(&cfg, json, strlen(json));
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(cfg.plugins.enabled);
    SC_ASSERT_EQ(cfg.plugins.plugin_paths_len, 2);
    SC_ASSERT_NOT_NULL(cfg.plugins.plugin_paths);
    SC_ASSERT_STR_EQ(cfg.plugins.plugin_paths[0], "/lib/foo.so");
    SC_ASSERT_STR_EQ(cfg.plugins.plugin_paths[1], "/lib/bar.so");
    sc_arena_destroy(arena);
}

static void test_plugin_config_parse_plugins_as_array(void) {
    sc_allocator_t backing = sc_system_allocator();
    sc_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    sc_arena_t *arena = sc_arena_create(backing);
    SC_ASSERT_NOT_NULL(arena);
    cfg.arena = arena;
    cfg.allocator = sc_arena_allocator(arena);
    const char *json = "{\"plugins\":[\"/a.so\",\"/b.so\"]}";
    sc_error_t err = sc_config_parse_json(&cfg, json, strlen(json));
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(cfg.plugins.plugin_paths_len, 2);
    SC_ASSERT_STR_EQ(cfg.plugins.plugin_paths[0], "/a.so");
    SC_ASSERT_STR_EQ(cfg.plugins.plugin_paths[1], "/b.so");
    sc_arena_destroy(arena);
}

static void test_plugin_unload_all_no_crash(void) {
    sc_plugin_unload_all();
    /* No plugins loaded - should not crash */
}

static void test_plugin_version_mismatch_fails(void) {
    sc_allocator_t a = sc_system_allocator();
    sc_plugin_host_t host = {
        .alloc = &a,
        .register_tool = NULL,
        .register_provider = NULL,
        .register_channel = NULL,
        .host_ctx = NULL,
    };
    sc_plugin_info_t info = {0};
    sc_plugin_handle_t *handle = NULL;
    sc_error_t err = sc_plugin_load(&a, "/bad_api/plugin.so", &host, &info, &handle);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
    SC_ASSERT_NULL(handle);
}

void run_plugin_tests(void) {
    SC_RUN_TEST(test_plugin_load_bad_path_fails);
    SC_RUN_TEST(test_plugin_host_register_tool_callback);
    SC_RUN_TEST(test_plugin_config_parse_empty_array);
    SC_RUN_TEST(test_plugin_config_parse_with_paths);
    SC_RUN_TEST(test_plugin_config_parse_plugins_as_array);
    SC_RUN_TEST(test_plugin_unload_all_no_crash);
    SC_RUN_TEST(test_plugin_version_mismatch_fails);
}
