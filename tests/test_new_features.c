/* Tests for PDF tool, health endpoints, config reload, ws_streaming. */
#include "seaclaw/config.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/arena.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/json.h"
#include "seaclaw/core/string.h"
#include "seaclaw/gateway.h"
#include "seaclaw/tool.h"
#include "seaclaw/tools/pdf.h"
#include "test_framework.h"
#include <string.h>

static void test_pdf_create(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool = {0};
    sc_error_t err = sc_pdf_create(&alloc, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable);
    SC_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "pdf");
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_pdf_missing_path(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool = {0};
    sc_pdf_create(&alloc, &tool);
    sc_json_value_t *args = sc_json_object_new(&alloc);
    sc_tool_result_t result = {0};
    tool.vtable->execute(tool.ctx, &alloc, args, &result);
    sc_json_free(&alloc, args);
    SC_ASSERT_FALSE(result.success);
    if (result.output_owned && result.output)
        alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    if (result.error_msg_owned && result.error_msg)
        alloc.free(alloc.ctx, (void *)result.error_msg, result.error_msg_len + 1);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_pdf_test_mode(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool = {0};
    sc_pdf_create(&alloc, &tool);
    sc_json_value_t *args = sc_json_object_new(&alloc);
    sc_json_object_set(&alloc, args, "path", sc_json_string_new(&alloc, "/tmp/test.pdf", 13));
    sc_tool_result_t result = {0};
    tool.vtable->execute(tool.ctx, &alloc, args, &result);
    sc_json_free(&alloc, args);
    SC_ASSERT_TRUE(result.success);
    SC_ASSERT_NOT_NULL(result.output);
    SC_ASSERT_TRUE(strstr(result.output, "test.pdf") != NULL);
    if (result.output_owned && result.output)
        alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_pdf_null_args(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool = {0};
    sc_pdf_create(&alloc, &tool);
    sc_tool_result_t result = {0};
    sc_error_t err = tool.vtable->execute(tool.ctx, &alloc, NULL, &result);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_health_endpoints_exist(void) {
    SC_ASSERT_TRUE(sc_gateway_path_is("/health", "/health"));
    SC_ASSERT_TRUE(sc_gateway_path_is("/healthz", "/healthz"));
    SC_ASSERT_TRUE(sc_gateway_path_is("/ready", "/ready"));
    SC_ASSERT_TRUE(sc_gateway_path_is("/readyz", "/readyz"));
    SC_ASSERT_TRUE(sc_gateway_path_is("/health/", "/health"));
    SC_ASSERT_FALSE(sc_gateway_path_is("/healthcheck", "/health"));
    SC_ASSERT_FALSE(sc_gateway_path_is(NULL, "/health"));
    SC_ASSERT_FALSE(sc_gateway_path_is("/health", NULL));
}

static void test_config_reload_flag(void) {
    SC_ASSERT_FALSE(sc_config_get_and_clear_reload_requested());
    sc_config_set_reload_requested();
    SC_ASSERT_TRUE(sc_config_get_and_clear_reload_requested());
    SC_ASSERT_FALSE(sc_config_get_and_clear_reload_requested());
}

static void test_ws_streaming_config(void) {
    sc_allocator_t backing = sc_system_allocator();
    sc_config_t cfg = {0};
    sc_arena_t *arena = sc_arena_create(backing);
    cfg.arena = arena;
    cfg.allocator = sc_arena_allocator(arena);
    SC_ASSERT_FALSE(sc_config_get_provider_ws_streaming(&cfg, "openai"));
    SC_ASSERT_FALSE(sc_config_get_provider_ws_streaming(NULL, "openai"));
    const char *json =
        "{\"providers\":[{\"name\":\"openai\"},{\"name\":\"test\",\"ws_streaming\":true}]}";
    sc_config_parse_json(&cfg, json, strlen(json));
    SC_ASSERT_FALSE(sc_config_get_provider_ws_streaming(&cfg, "openai"));
    SC_ASSERT_TRUE(sc_config_get_provider_ws_streaming(&cfg, "test"));
    sc_config_deinit(&cfg);
}

static void test_suppress_tool_progress(void) {
    sc_allocator_t backing = sc_system_allocator();
    sc_config_t cfg = {0};
    sc_arena_t *arena = sc_arena_create(backing);
    cfg.arena = arena;
    cfg.allocator = sc_arena_allocator(arena);
    SC_ASSERT_FALSE(cfg.channels.suppress_tool_progress);
    const char *json = "{\"channels\":{\"suppress_tool_progress\":true}}";
    sc_config_parse_json(&cfg, json, strlen(json));
    SC_ASSERT_TRUE(cfg.channels.suppress_tool_progress);
    sc_config_deinit(&cfg);
}

void run_new_features_tests(void) {
    SC_TEST_SUITE("New Features - PDF");
    SC_RUN_TEST(test_pdf_create);
    SC_RUN_TEST(test_pdf_missing_path);
    SC_RUN_TEST(test_pdf_test_mode);
    SC_RUN_TEST(test_pdf_null_args);
    SC_TEST_SUITE("New Features - Health");
    SC_RUN_TEST(test_health_endpoints_exist);
    SC_TEST_SUITE("New Features - Config");
    SC_RUN_TEST(test_config_reload_flag);
    SC_RUN_TEST(test_ws_streaming_config);
    SC_RUN_TEST(test_suppress_tool_progress);
}
