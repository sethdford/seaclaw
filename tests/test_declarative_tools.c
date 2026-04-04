#include "human/tools/declarative.h"
#include "human/core/allocator.h"
#include "test_framework.h"

static void declarative_discover_null_args(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_declarative_tool_def_t *defs = NULL;
    size_t count = 0;
    HU_ASSERT_EQ(hu_declarative_tools_discover(NULL, "/tmp", &defs, &count), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_declarative_tools_discover(&a, NULL, &defs, &count), HU_ERR_INVALID_ARGUMENT);
}

static void declarative_discover_test_mode(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_declarative_tool_def_t *defs = NULL;
    size_t count = 99;
    HU_ASSERT_EQ(hu_declarative_tools_discover(&a, "/tmp/nonexistent", &defs, &count), HU_OK);
    HU_ASSERT_EQ(count, 0);
    HU_ASSERT_NULL(defs);
}

static void declarative_create_and_query(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_declarative_tool_def_t def = {0};
    def.name = "weather";
    def.description = "Get weather";
    def.parameters_json = "{\"type\":\"object\"}";
    def.exec_type = HU_DECL_EXEC_HTTP;
    def.exec_url = "https://api.example.com/weather";
    def.exec_method = "GET";
    hu_tool_t tool;
    HU_ASSERT_EQ(hu_declarative_tool_create(&a, &def, &tool), HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "weather");
    if (tool.vtable->deinit) tool.vtable->deinit(tool.ctx, &a);
}

static void declarative_def_free_null(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_declarative_tool_def_free(NULL, &a);
}

void run_declarative_tools_tests(void) {
    HU_TEST_SUITE("DeclarativeTools");
    HU_RUN_TEST(declarative_discover_null_args);
    HU_RUN_TEST(declarative_discover_test_mode);
    HU_RUN_TEST(declarative_create_and_query);
    HU_RUN_TEST(declarative_def_free_null);
}
