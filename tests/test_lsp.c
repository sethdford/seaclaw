#include "test_framework.h"
#include "human/tools/lsp.h"

#include <string.h>

static void test_lsp_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = hu_lsp_tool_create(&alloc);
    HU_ASSERT(tool.vtable != NULL);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "lsp");
}

static void test_lsp_execute(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = hu_lsp_tool_create(&alloc);
    hu_json_value_t args; memset(&args, 0, sizeof(args));
    hu_tool_result_t result; memset(&result, 0, sizeof(result));
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, &args, &result), HU_OK);
    HU_ASSERT(result.success);
}

static void test_lsp_tool_create_null_allocator_returns_invalid(void) {
    hu_tool_t tool = hu_lsp_tool_create(NULL);
    HU_ASSERT(tool.vtable == NULL);
    HU_ASSERT(tool.ctx == NULL);
}

static void test_lsp_tool_deinit_uninitialized_no_crash(void) {
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    hu_allocator_t alloc = hu_system_allocator();
    if (tool.vtable && tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

void run_lsp_tests(void) {
    HU_TEST_SUITE("LSP Tool");
    HU_RUN_TEST(test_lsp_create);
    HU_RUN_TEST(test_lsp_execute);
    HU_RUN_TEST(test_lsp_tool_create_null_allocator_returns_invalid);
    HU_RUN_TEST(test_lsp_tool_deinit_uninitialized_no_crash);
}

